/*
Jose Renteria
951742079

This is my own work, apart from examples taken from the handout.


*/
#include "bxp/bxp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <valgrind/valgrind.h>
#include "ADTs/heapprioqueue.h"
#include "ADTs/queue.h"
#include "ADTs/hashmap.h"

#define UNUSED __attribute__((unused))
#define PORT 19999
#define SERVICE "DTS"

#define USECS (10 * 1000)

unsigned long global_id = 1;
pthread_mutex_t lock;
// Shared Data structures
const PrioQueue *PQ = NULL;
const Map *map = NULL;


typedef struct event 
{
    int oneShot;
    int repeat;
    int cancel;
    unsigned long n_repeats;
    unsigned long interval;
    unsigned long id;
    unsigned long clid;
    unsigned int port;
    char *host; 
    char *service;
} Event;

long int hash_fxn(void *key, long int N)
{ 
    return (( (unsigned long) key) %N);
}

/* compare two timeval structs
    three cases for return:
        i. -1 if p2 > p1
        ii. 1 if p1 > p2
        iii. 0 if p1 == p2
*/
int compare(void *p1, void *p2)
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

// compare function for map
int compare_map(void *id1, void *id2)
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
int extractWords(char *buf, char *sep, char *words[]) 
{
    int i;
    char *p;

    for (p = strtok(buf, sep), i = 0; p != NULL; p = strtok(NULL, sep), i++)
        words[i] = p;
    words[i] = NULL;
    return i;
}

unsigned long insert_oneshot(char **words)
{
    Event *event = (Event *)malloc(sizeof(Event));
    struct timeval *t = (struct timeval *)malloc(sizeof(struct timeval));

    // initialize timeval
    sscanf(words[2], "%ld", &(t->tv_sec));
    sscanf(words[3], "%ld", &(t->tv_sec));
    
    // init struct variables
    event->id = global_id;
    event->oneShot = 1;
    event->repeat = 0;
    event->cancel = 0;
    event->n_repeats = 0;
    event->interval = 0;
    // get clid and port
    sscanf(words[1], "%lu", &(event->clid));
    sscanf(words[6], "%u", &(event->port));
    // duplicate to event struct vars
    event->host = strdup(words[4]);
    event->service = strdup(words[5]);

    // insert into the priority queue where the priority is the time elapsed since Epoch
    PQ->insert(PQ, t, (void *)event);

    // insert map with id as key
    map->putUnique(map, (void *)global_id, (void *)event);

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
    // get ms
    sscanf(words[2], "%ld", &ms);
    us = ms * 1000;

    while(us > 1000000)
    {
        us -= 1000000;
        s++;
    }


    // init timeval struct
    t->tv_sec = s;
    t->tv_usec = us;

    // init event struct
    event->id = global_id;
    event->oneShot = 0;
    event->repeat = 1;
    event->cancel = 0;
    // get n_repeats, interval, clid, and port
    sscanf(words[3], "%lu", &(event->n_repeats));
    sscanf(words[2], "%lu", &(event->interval));
    sscanf(words[1], "%lu", &(event->clid));
    sscanf(words[6], "%u", &(event->port));
    
    event->host = strdup(words[4]);
    event->service = strdup(words[5]);

    // insert priority queue where the priority is the time since Epoch
    PQ->insert(PQ, t, (void *)event);

    // insert map with id as key
    map->putUnique(map, (void *)global_id, (void *)event);

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

    // get event from map
    if(map->get(map, (void *) current, (void **) &event_ptr))
    {
        event_ptr->cancel = 1;
    }
    // set return value to the global id
    ret = global_id;
    // increment global id
    global_id++;

    return ret;
}

void *dtsv3()
{
    
    BXPEndpoint sender;
    BXPService bxps;

    char *query = (char *)malloc(BUFSIZ);
    char *resp = (char *)malloc(BUFSIZ);

    unsigned len;
    char *service;
    unsigned short port;

    char *words[25];
    int N;
    unsigned long svid;

    service = SERVICE;
    port = PORT;
    
    //Initialize BXP system - bind to ‘port’ if non-zero
    assert(bxp_init(port, 1));
    
    //Offer service named `service' in this process
    bxps = bxp_offer(service);
    
    if (bxps == NULL) 
    {
        fprintf(stderr, "Failure offering Echo service\n");
        free(query);
        free(resp);
        exit(EXIT_FAILURE);
    }
    //obtain the next query message from `bxps' - blocks until message available
    while ((len = bxp_query(bxps, &sender, query, BUFSIZ)) > 0) 
    {
        char cmd[1000];
        query[len] = '\0';
        strcpy(cmd, query);
        N = extractWords(cmd, "|", words);
        if((strcmp(words[0], "OneShot") == 0) && N == 7)
        {
                svid = insert_oneshot(words);
                VALGRIND_MONITOR_COMMAND("leak_check summary");
        }
        else if((strcmp(words[0], "Repeat") == 0) && N == 7)
        {
                svid = insert_repeat(words);
                VALGRIND_MONITOR_COMMAND("leak_check summary");
        }
        else if((strcmp(words[0], "Cancel") == 0) && N == 2)
        {
                svid = cancel_update(words);
                VALGRIND_MONITOR_COMMAND("leak_check summary");
        }
        else
        {
            svid = 0;  
        }

        if(svid == 0)
            strcpy(resp, "0");
        else
            sprintf(resp, "1%08lu", svid);
        
        bxp_response(bxps, &sender, resp, strlen(resp) + 1);
    }
    free(query);
    free(resp);

    pthread_exit(NULL);
}

void *timer(UNUSED void *args) 
{
    struct timeval now;
    struct timeval *later = (struct timeval *)malloc(sizeof(struct timeval));
    
    while(1) 
    {
        
        usleep(USECS);
        gettimeofday(&now, NULL);

        //Queue to store ready to harvest events
        const Queue *queue = Queue_create(NULL);
        //Queue to store the repeat events
        const Queue *r_queue = Queue_create(NULL);

        struct timeval *tval_ptr;

        Event *eve_ptr;

        /* Get the events that are ready to harvest,
           Then add to queue */
        pthread_mutex_lock(&lock);
        while(PQ->min(PQ, (void **)&tval_ptr, (void **)&eve_ptr))
        {
            //If now >= tval_ptr
            if(compare((void *)tval_ptr, (void *)&now) <= 0)
            {
                //Remove the minimum event from the priority queue
                PQ->removeMin(PQ, (void **)&tval_ptr, (void **)&eve_ptr);
                //Add to queue
                queue->enqueue(queue, eve_ptr);
                //TODO: GIVES ERROR WHEN REPEAT CALLED: 
                //free(tval_ptr);
            }
            else
                break;
        }
        pthread_mutex_unlock(&lock);
        
        /* Process the events that are in the queue */
        while(queue->dequeue(queue, (void **)&eve_ptr))
        {
            //If the event is not cancel, fire it

            if(eve_ptr->cancel == 0)
            {
                printf("Event fired: %lu|%s|%s|%u\n", eve_ptr->clid, eve_ptr->host, eve_ptr->service, eve_ptr->port);
                if(eve_ptr->repeat == 1 && eve_ptr->n_repeats > 1)
                {
                    eve_ptr->n_repeats--;
                    r_queue->enqueue(r_queue, (void *)eve_ptr);
                }
                else
                {
                    map->remove(map, (void *)&eve_ptr->id);
                    free(tval_ptr);
                    free(eve_ptr->host);
                    free(eve_ptr->service);
                    free(eve_ptr);
                }            
                
            }
            //If it is cancel, recycle the heap storage
            else{
                map->remove(map, (void *)&eve_ptr->id);
                free(tval_ptr);
                free(eve_ptr->host);
                /ree(eve_ptr->service);
                free(eve_ptr);
            }
        }

        /* Go through r_queue and insert the repeat events
           to the priority queue with a new priority */
        pthread_mutex_lock(&lock);
        while(r_queue->dequeue(r_queue, (void **)&eve_ptr))
        {
            //later = now + interval
            later->tv_sec = now.tv_sec;
            later->tv_usec = now.tv_usec + eve_ptr->interval*1000;
            while(later->tv_usec > 1000000)
            {
                later->tv_usec = later->tv_usec - 1000000;
                later->tv_sec++;
            }
            //Insert back into the pq with later as a priority
            if(eve_ptr->cancel == 0)
                PQ->insert(PQ, (void *)later, (void *)eve_ptr);
        }
        pthread_mutex_unlock(&lock);
        queue->destroy(queue);
        r_queue->destroy(r_queue);
    }
    free(later);
    pthread_exit(NULL);
}

int main(UNUSED int argc, UNUSED char *argv[]) 
{
    PQ = HeapPrioQueue(compare, NULL, NULL);
    if(PQ == NULL) 
    {
        fprintf(stderr, "ERROR: Piority Queue creation failed.\n");
    }
    map = HashMap(128, 4.0, hash_fxn, compare_map, NULL, NULL);
    if(map == NULL) 
    {
        fprintf(stderr, "ERROR: HashMap creation failed.\n");
    }
    
    pthread_t receiver_t, timer_t;
    pthread_mutex_init(&lock, NULL);
    
    pthread_create(&timer_t, NULL, timer, NULL);
    pthread_create(&receiver_t, NULL, dtsv3, NULL);
    
    pthread_join(receiver_t, NULL);
    pthread_join(timer_t, NULL);

    PQ->destroy(PQ);
    map->destroy(map);

    return 0;
}