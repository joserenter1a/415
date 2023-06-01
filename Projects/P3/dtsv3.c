/*
Jose Renteria
951742079

This is my own work, apart from examples taken from the handout.


*/
// shared includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <valgrind/valgrind.h>

#include "bxp/bxp.h"
#include "ADTs/heapprioqueue.h"
#include "ADTs/queue.h"
#include "ADTs/hashmap.h"
// Macros
#define UNUSED __attribute__((unused))
#define PORT 19999
#define SERVICE "DTS"
#define USECS (10 * 1000)

// init the shared data structures and set to NULL
const PrioQueue *PQ = NULL;
const Map *hashmap = NULL;

unsigned long global_id = 1;
pthread_mutex_t mutex;

// define a struct to hold our event data
typedef struct event 
{
    int oneShot;
    int repeat;
    int cancel;
    unsigned long n_repeats;
    unsigned long t_interval;
    unsigned long id;
    unsigned long clid; 
    unsigned int port;
    char *host;
    char *service;
} Event;

// from CADS 12.3
long int hash_function(void *key, long int N)
{
    return (((unsigned long)key)%N);
}

/* compare two timeval structs
    three cases for return:
        i. -1 if p2 > p1
        ii. 1 if p1 > p2
        iii. 0 if p1 == p2
*/
int compare_function(void *p1, void *p2)
{
    struct timeval *s1 = (struct timeval *)p1;
    struct timeval *s2 = (struct timeval *)p2;
    int cmp; 
    if((s1->tv_sec) < s2->tv_sec)
    {
        cmp = -1;
    }
    else if((s1->tv_sec) > (s2->tv_sec))
    {
        cmp = 1;
    }
    else
    {
        if((s1->tv_sec) < (s2->tv_sec))
        {
            cmp = -1;
        }
        else if((s1->tv_sec) > (s2->tv_sec))
        {
            cmp = 1;
        }
        else 
        {
            cmp = 0;
        }
    }
    return cmp;
}

// compare function for hashmap
int map_compare_function(void *id1, void *id2)
{
    unsigned long *s1 = (unsigned long *)id1;
    unsigned long *s2 = (unsigned long *)id2;
    int cmp;

    if(s1 > s2) {cmp = 1;}
    else if (s2 > s1) {cmp = -1;}
    else {cmp = 0;}

    return cmp;
}

// This function was given in lab_7/cbserver.c
int extractWords(char *buf, char *sep, char *words[]) {
    int i;
    char *p;

    for (p = strtok(buf, sep), i = 0; p != NULL; p = strtok(NULL, sep), i++)
    {
        words[i] = p;
    }
    words[i] = NULL;
    return i;
}

unsigned long insert_oneshot(char **words)
{
    // create our timeval structu
    struct timeval *t = (struct timeval *)malloc(sizeof(struct timeval));
    // create event struct
    Event *event = (Event *)malloc(sizeof(Event));

    // initialize timeval
    sscanf(words[2], "%ld", &(t->tv_sec));
    sscanf(words[3], "%ld", &(t->tv_sec));

    // initialize event struct
    event->id = global_id;
    event->oneShot = 1;
    event->repeat = 0;
    event->cancel = 0;
    event->n_repeats = 0;
    event->t_interval = 0;

    sscanf(words[1], "%lu", &(event->clid));
    sscanf(words[6], "%u", &(event->port));

    event->host = strdup(words[4]);
    event->service = strdup(words[5]);

    // insert into the priority queue where the priority is the time elapsed since Epoch
    PQ->insert(PQ, t, (void *)event);

    // insert hashmap with id as key
    hashmap->putUnique(hashmap, (void *)global_id, (void *)event);

    // increment id for the next event
    global_id++;

    return event->id;
}

unsigned long insert_repeat(char **words)
{
    Event *event = (Event *)malloc(sizeof(Event));
    struct timeval *t = (struct timeval *)malloc(sizeof(struct timeval));

    // convert msecs to secs and usecs
    long s = 0;
    long ms = 0;
    long us = 0;
    sscanf(words[2], "%ld", &ms);
    us = ms * 1000;
    while(us > 1000000)
    {
        us -= 1000000;
        s++;
    }


    // initialize timeval struct
    t->tv_sec = s;
    t->tv_usec = us;

    // initialize event struct
    event->id = global_id;
    event->oneShot = 0;
    event->repeat = 1;
    event->cancel = 0;
    // get clid, intercal, repeats, and port
    sscanf(words[1], "%lu", &(event->clid));
    sscanf(words[2], "%lu", &(event->t_interval));
    sscanf(words[3], "%lu", &(event->n_repeats));
    sscanf(words[6], "%u", &(event->port));

    event->host = strdup(words[4]);
    event->service = strdup(words[5]);

    // insert priority queue where the priority is the time since Epoch
    PQ->insert(PQ, t, (void *)event);

    // insert hashmap with id as key
    hashmap->putUnique(hashmap, (void *)global_id, (void *)event);

    global_id++;

    return event->id;

}

unsigned long cancel_update(char **words)
{
    Event *event_ptr;
    unsigned long current = 0;
    unsigned long ret = 0;
    // get id of event to be cancelled

    sscanf(words[1], "%lu", &(current));
    // get event from hashmap
    if(hashmap->get(hashmap, (void *)current, (void **)&event_ptr))
    {
        event_ptr->cancel = 1;
    }
    ret = global_id;
    global_id++;

    return ret;
}

// Built on top of dtsv2
// receiver function
void *dtsv3()
{
    // Initialize BXP Endpoint and Service
    BXPEndpoint send;
    BXPService bxp_service;

    char *server_query = (char *)malloc(BUFSIZ);
    char *server_response = (char *)malloc(BUFSIZ + 1);
    char *query_copy = (char *)malloc(sizeof(server_query));
    char *command;

    char *words[25];

    unsigned query_length, response_length;
    unsigned long svid;

    int N;

    // Initialize our BXP protocol, bound to our local Port
    assert(bxp_init(PORT, 1));

    // Offer our BXP Service
    bxp_service = bxp_offer(SERVICE);

    // If the service is null, exit and free
    if(bxp_service == NULL)
    {
        fprintf(stderr, "Failure to initiailize service\n");
        free(server_response);
        free(server_query);
        exit(EXIT_FAILURE);
    }

    while((query_length = bxp_query(bxp_service, &send, server_query, BUFSIZ)) > 0)
    {
        // parse our query string
        server_query[query_length] = '\0';
        // copy the query
        strcpy(query_copy, server_query);
        command = query_copy;

        N = extractWords(command, "|", words);

        VALGRIND_MONITOR_COMMAND("leak_check summary");

        if(strcmp(words[0], "OneShot") == 0)
        {
            switch(N)
            {
                case 7:
                    svid = insert_oneshot(words);
                    sprintf(server_response, "1%08lu", svid);
                    //fprintf(stdout, "%s - Server Request Success\n", server_response);
                    break;
                default:
                    sprintf(server_response, "0%s", server_query);
            }
            VALGRIND_MONITOR_COMMAND("leak_check summary");        
        }
        else if(strcmp(words[0], "Repeat") == 0)
        {
            sprintf(server_response, "0%s", server_query);
            /*
            switch(N)
            {
                case 7:
                    svid = insert_repeat(words);
                    sprintf(server_response, "0%s", server_query);
                    fprintf(stdout, "%s - Server Request Success\n", server_response);
                    break;
                default:
                    sprintf(server_response, "0%s", server_query);
            }*/

            VALGRIND_MONITOR_COMMAND("leak_check summary");
        }
        
        else if(strcmp(words[0], "Cancel") == 0)
        {
            switch(N)
            {
                case 2:
                    svid = cancel_update(words);
                    sprintf(server_response, "1%08lu", svid);
                    //fprintf(stdout, "%s - Server Request Success\n", server_response);
                    break;
                default:
                    sprintf(server_response, "0%s", server_query);
            }
            VALGRIND_MONITOR_COMMAND("leak_check summary");
        }
        else
        {
            sprintf(server_response, "0%s", server_query);
        }
        VALGRIND_MONITOR_COMMAND("leak_check summary");
        response_length = strlen(server_response) + 1;
        bxp_response(bxp_service, &send, server_response, response_length);
    }   

    free(server_query);
    free(server_response);
    pthread_exit(NULL);
}

void *timer_function(UNUSED void *args)
{
    struct timeval instant;
    struct timeval *future = (struct timeval *)malloc(sizeof(struct timeval));

    while(1)
    {
        usleep(USECS);
        gettimeofday(&instant, NULL);

        //Queue to store events that are ready to harvest
        const Queue *ready_q = Queue_create(NULL);

        // Queue to store repeat events
        const Queue *repeat_q = Queue_create(NULL);

        struct timeval *timeval_ptr;

        Event *event_ptr;

        // Get events that are ready to harvest, add to ready_q
        pthread_mutex_lock(&mutex);
        while(PQ->min(PQ, (void **)&timeval_ptr, (void **)&event_ptr))
        {
            // if instant >= timeval_ptr
            if(compare_function((void *)timeval_ptr, (void *)&instant) <= 0)
            {
                PQ->removeMin(PQ, (void**)&timeval_ptr, (void **)&event_ptr);
                // add to queue
                ready_q->enqueue(ready_q, event_ptr);
                // free(timeval_ptr);
            }
            else
            {
                break;
            }
        }
            pthread_mutex_unlock(&mutex);
            
            // process events in the queue
            while(ready_q->dequeue(ready_q, (void **)&event_ptr))
            {
                // if event not canceled, fire
                if(event_ptr->cancel == 0)
                {
                    fprintf(stdout, "Event fired: %lu|%s|%s|%u\n", event_ptr->clid, event_ptr->host, event_ptr->service, event_ptr->port);
                    if(event_ptr->repeat == 1 && event_ptr->n_repeats > 1)
                    {
                        event_ptr->n_repeats--;
                        repeat_q->enqueue(repeat_q, (void *)event_ptr);
                    }
                    else
                    {
                        hashmap->remove(hashmap, (void *)&event_ptr->id);
                        free(timeval_ptr);
                        free(event_ptr->host);
                        free(event_ptr->service);
                        free(event_ptr);
                    }
                }
                else 
                {
                    hashmap->remove(hashmap, (void *)&event_ptr->id);
                    free(timeval_ptr);
                    free(event_ptr->host);
                    free(event_ptr->service);
                    free(event_ptr);
                }
            }
        // lock mutex
        pthread_mutex_lock(&mutex);
        while(repeat_q->dequeue(repeat_q, (void**)&event_ptr))
        {
            future->tv_sec = instant.tv_sec;
            future->tv_usec = instant.tv_usec;
            while(future->tv_usec > 1000000)
            {
                future->tv_usec = future->tv_usec - 1000000;
                future->tv_sec++;
            }
            if(event_ptr->cancel == 0)
            {
                PQ->insert(PQ, (void *)future, (void *)event_ptr);
            }

        }
        pthread_mutex_unlock(&mutex);
        ready_q->destroy(ready_q);
        repeat_q->destroy(repeat_q);
    }
    free(future);
    pthread_exit(NULL);
}


int main(UNUSED int argc, UNUSED char *argv[])
{
    // create our priority queue
    PQ = HeapPrioQueue(compare_function, NULL, NULL);
    if(PQ == NULL)
    {
        fprintf(stderr, "Failure to create Priority Queue\n");
    }

    // create our hashmap
    hashmap = HashMap(128, 4.0, hash_function, map_compare_function, NULL, NULL);
    if(hashmap == NULL)
    {
        fprintf(stderr, "Failure to create hashmap\n");

    }
    pthread_t receiver_thread, timer_thread;
    // initialize mutex lock to be used
    pthread_mutex_init(&mutex, NULL);

    // create our timer thread and receiver thread
    pthread_create(&timer_thread, NULL, timer_function, NULL);
    pthread_create(&receiver_thread, NULL, dtsv3, NULL);

    // join threads
    pthread_join(receiver_thread, NULL);
    pthread_join(timer_thread, NULL);
    // cleanup data structures
    PQ->destroy(PQ);
    hashmap->destroy(hashmap);

    return 0;
}