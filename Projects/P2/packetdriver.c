/*
Author: Jose Renteria

Citations:
The Bounded Buffer Problem - Neso Academy
https://www.youtube.com/watch?v=Qx3P2wazwI0&t=611s

*/

#include "packetdriver.h"
#include "freepacketdescriptorstore__full.h"
#include "BoundedBuffer.h"
#include "diagnostics.h"
#include <pthread.h>
#include <sched.h>

#define RECEIVER_POOL 4
#define RECEIVED_BUF_SIZ 2
#define SENDING_LIMIT 3
//a queue of packet descriptors waiting to be sent
static BoundedBuffer *waiting_PDs; 
//a queue of packet descriptor pool for receiving
static BoundedBuffer *PD_receiver_pool;
// a queue of packet descriptors waiting to be received
static BoundedBuffer *waiting_receives[MAX_PID+1];

// sending thread
static pthread_t sending_thread;
// receiving thread
static pthread_t receiving_thread;

// create sending and receiving structs that pass pointers to the data required by 
// creating threads via void * pointer, this will allow us to create static data
struct send_arg_t
{
	FreePacketDescriptorStore *free_pds;
	NetworkDevice *nw_device;
	BoundedBuffer *waiting_PDs;
	BoundedBuffer *PD_receiver_pool;
};

struct receive_arg_t
{
	FreePacketDescriptorStore *free_pds;
	NetworkDevice *nw_device;
	BoundedBuffer **waiting_receives;
	BoundedBuffer *PD_receiver_pool;
};

static struct send_arg_t send_args;
static struct receive_arg_t receive_args;

/*
We need to be able to maintain the contents of the receiver pool, so 
we can create a helper function that helps us return a free packet descriptor
 in a specified order. The logic works as follows:

	i. An attempt to return to the BoundedBuffer pool is made
	ii. If the attempt fails, execute a nonblocking call to be returned
		to the FreePacketDescriptorStore/
		since free_pds is unbounded, executing nonblocking_put_packet should work
*/ 
static int nonblocking_put_pd(BoundedBuffer *pool, FreePacketDescriptorStore *free_pds,
							PacketDescriptor *packet_descriptor)
			{
				int res;
				if(! (res = pool->nonblockingWrite(pool, packet_descriptor)))
				{
					res = free_pds->nonblockingPut(free_pds, packet_descriptor);
				}
				return res; 
			}

/*
We need a function to send threads, the logic works as follows:

	initialize an infinite loop, each iteration
		i. get the next message to be placed on network
		ii. try to send the message up to 3 times until it succeeds
		iii. record a send success or send failure
		iv. try to add the packet descriptor to the receiver pool,
			if the receiver pool is full, then we place it in the
			free packet descriptor store

*/
void *send_thread (void *args)
{
	PacketDescriptor *message = NULL;
	int res, nsends;
	struct send_arg_t *argvec = (struct send_arg_t *) args;

	for(;;)
	{
		argvec->waiting_PDs->blockingRead(argvec->waiting_PDs, (void**) &message);
		for(nsends = 0; nsends < SENDING_LIMIT; nsends++)
		{
			if((res = argvec->nw_device->sendPacket(argvec->nw_device, message)))
			{
				break;
			}
		}
		if (res)
		{
			DIAGNOSTICS("DRIVER => Info: Packet send success after %d attempts\n", ++nsends);
		}
		else
		{
			DIAGNOSTICS("DRIVER => Info: Packet send failure after %d attempts\n", nsends);
		}
		if(!nonblocking_put_pd(argvec->PD_receiver_pool, argvec->free_pds, message))
		{
			DIAGNOSTICS("Driver => Error: Failure to return packet descriptor\n");
		}
	}
	return NULL;
}

/*
Now we need to be able to get a single free packet descriptor
The logic works as follows:
	
	i. Attempt to get free pd from the BoundedBuffer pool
	ii. If unsuccessful, perform a nonblocking call and get one from the
		free packet descriptor store
*/

static int nonblocking_get_pd(BoundedBuffer *pool, FreePacketDescriptorStore *free_pds, PacketDescriptor **PD)
{
	int res;
	if(! (res = pool->nonblockingRead(pool, (void**) PD)))
	{
		res = free_pds->nonblockingGet(free_pds, PD);
	}
	return res;
}

/*

We need a thread receiver function, to constantly wait to read packets from 
our network. When a pakcet is received, it makes a decision in order to make sure our
program can keep up with multiple packets coming in.

this will help prevent congestion and packet loss during periods of high traffic.
*/

void *receive_thread(void *args)
{
	struct receive_arg_t *argvec = (struct receive_arg_t *) args;
	PacketDescriptor *curr_pd, *full_pd;
	PID p_pid;
	
	/*
		Follow the pattern:
			i. obtain
			ii. init
			iii. register

	*/
	argvec->free_pds->blockingGet(argvec->free_pds, &curr_pd);
	initPD(curr_pd);
	argvec->nw_device->registerPD(argvec->nw_device, curr_pd);
	//infinite loop
	for(;;)
	{
		// wait to receive a packet
		argvec->nw_device->awaitIncomingPacket(argvec->nw_device);
		// pd was filled
		full_pd = curr_pd;
		if(nonblocking_get_pd(argvec->PD_receiver_pool, argvec->free_pds, &curr_pd))
		{
			// success, obtained another packet descriptor
			// now, init & regiseter
			initPD(curr_pd);
			argvec->nw_device->registerPD(argvec->nw_device, curr_pd);
			// we can now store our full packet descriptor in a buffer
			p_pid = getPID(full_pd);
			DIAGNOSTICS("DRIVER => Packet receive successful. Ready for application %u\n", p_pid);
			if(!argvec->waiting_receives[p_pid]->nonblockingWrite(argvec->waiting_receives[p_pid], (void *)full_pd))
			{
				// packet does not fit, must return to free packet descriptor store
				DIAGNOSTICS("DRIVER => Application (%u) PD Store full, deleting data.\n", p_pid);
				if(!nonblocking_put_pd(argvec->PD_receiver_pool, argvec->free_pds, full_pd))
				{
					DIAGNOSTICS("DRIVER =>  Error: Cannot return PD to store");
				}
				else
				{
					// could not get another PD, reuse our full PD
					DIAGNOSTICS("DRIVER => No replacement PD, deleting data.\n");
					curr_pd = full_pd;
					initPD(curr_pd);
					argvec->nw_device->registerPD(argvec->nw_device, curr_pd);
				}
			}
			
		}
	}
	return NULL;
}

/*
The logic works as follows
 i. create FreePacketDescriptorStore using mem_start and mem_length
 ii. create any buffers required by your threads
 iii. create any threads required for implementation
 iv. return the free packet descriptor store to the code that called

*/

void init_packet_driver(NetworkDevice *nd, void *mem_start, unsigned long mem_length, FreePacketDescriptorStore **FPDS_ptr)
{
	int i, sb_len, num_PD;
	PacketDescriptor *PD;
	FreePacketDescriptorStore *FPDS;

	pthread_attr_t tattr;

	// first we setup FPDS
	FPDS = FreePacketDescriptorStore_create(mem_start, mem_length);
	*FPDS_ptr = (FreePacketDescriptorStore *)FPDS;
	num_PD = FPDS->size(FPDS);

		/*
			We can allow up to RECEIVER_BUF_SIZ packet descriptors to be queued for
			each application. Reserve RECEIVER_POOL packet descriptors for the receiving
			thread's free pool
			rest used for queuing sends
		*/
	sb_len = num_PD - RECEIVER_POOL- (MAX_PID + 1) * RECEIVED_BUF_SIZ;
	DIAGNOSTICS("Driver: %d packet descriptors are in receiver pool\n", RECEIVER_POOL);
	DIAGNOSTICS("Driver: %d received packet descriptors can be queued for %d applications\n", RECEIVED_BUF_SIZ, MAX_PID+1);
	DIAGNOSTICS("Driver: %d packet descriptors can be queued and sent", sb_len);

	// Build internal data structures
	waiting_PDs = BoundedBuffer_create(sb_len);
	for(i = 0; i<MAX_PID; i++)
	{
		waiting_receives[i] = BoundedBuffer_create(RECEIVED_BUF_SIZ);
	}
	// Now set up the packet descriptor pool for receiver use
	PD_receiver_pool = BoundedBuffer_create(RECEIVER_POOL);
	for(i = 0; i < RECEIVER_POOL; i ++)
	{
		FPDS->blockingGet(*FPDS_ptr, &PD);
		PD_receiver_pool->blockingWrite(PD_receiver_pool, PD);
	}

	// Now we can set up the argument structs for thread usage
	send_args.free_pds = FPDS;
	send_args.nw_device = nd;
	send_args.waiting_PDs = waiting_PDs;
	send_args.PD_receiver_pool = PD_receiver_pool;

	receive_args.free_pds = FPDS;
	receive_args.nw_device = nd;
	receive_args.waiting_receives = waiting_receives;
	receive_args.PD_receiver_pool = PD_receiver_pool;

	// Now we can start the threads, at default priority
	pthread_create(&sending_thread, NULL, &send_thread, (void *) &send_args);
	pthread_create(&receiving_thread, &tattr, &receive_thread, (void *) &receive_args);


}


// blocking routines for queuing IO reqs
void blocking_send_packet(PacketDescriptor *PD)
{
	waiting_PDs->blockingWrite(waiting_PDs, PD);
}

void blocking_get_packet(PacketDescriptor **PD, PID pid)
{
	waiting_receives[pid]->blockingRead(waiting_receives[pid], (void **) PD);
}

// Non blocking routines for queuing IO reqs

int nonblocking_send_packet(PacketDescriptor *PD)
{
	return waiting_PDs->nonblockingWrite(waiting_PDs, PD);
}

int nonblocking_get_packet(PacketDescriptor **PD, PID pid)
{
	return waiting_receives[pid]->nonblockingRead(waiting_receives[pid], (void **)PD);
}