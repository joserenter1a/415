/*
Jose Renteria
951742079

This is my own work, apart from examples taken from the handout.


*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
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
const Map *map = NULL;

unsigned long global_id = 1;
pthread_mutex_t lock;

// define a struct to hold our event data
typedef struct event 
{
    int oneShot;
    int repeat;
    int cancel;
    unsigned long num_repeats;
    unsigned long interval;
    unsigned long id;
    unsigned long clid; 
    unsigned int port;
    char *host;
    char *service;
} Event;

// from CADS 12.3
long int hashFxn(void *key, long int N)
{
    return (((unsigned long)key)%N);
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
int extractWords(char *buf, char *sep, char *words[]) {
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
    
    event->id = global_id;
    event->oneShot = 1;
    event->repeat = 0;
    event->cancel = 0;
    event->num_repeats = 0;
    event->interval = 0;
    sscanf(words[1], "%lu", &(event->clid));
    sscanf(words[6], "%u", &(event->port));
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
    sscanf(words[3], "%lu", &(event->num_repeats));
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
    if(map->get(map, (void *)current, (void **)&event_ptr))
    {
        event_ptr->cancel = 1;
    }
    ret = global_id;
    global_id++;

    return ret;
}

// Built on top of dtsv2
void *dtsv3()
{
    // Initialize BXP Endpoint and Service
    BXPEndpoint send;
    BXPService bxp_service;

    char *server_query = (char *)malloc(BUFSIZ);
    char *server_response = (char *)malloc(BUFSIZ + 1);
    char *query_copy = (char *)malloc(sizeof(server_query));
    char *command, *parsed;

    char *w[25];
    int N;

    unsigned query_length, response_length;
    unsigned long svid;

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

        N = extractWords(command, "|", w);

        VALGRIND_MONITOR_COMMAND("leak_check summary");

        if(strcmp(w[0], "OneShot") == 0)
        {
            switch(N)
            {
                case 7:
                    svid = insert_oneshot(w);
                    sprintf(server_response, "1%s", server_query);
                    fprintf(stdout, "%s - Server Request Success\n", server_response);
                    break;
                default:
                    sprintf(server_response, "0%s", server_query);
            }
            VALGRIND_MONITOR_COMMAND("leak_check summary");        
        }
        else if(strcmp(w[0], "Repeat") == 0)
        {
            switch(N)
            {
                case 7:
                    svid = insert_repeat(w);
                    sprintf(server_response, "1%s", server_query);
                    fprintf(stdout, "%s - Server Request Success\n", server_response);
                    break;
                default:
                    sprintf(server_response, "0%s", server_query);
            }
            VALGRIND_MONITOR_COMMAND("leak_check summary");
        }
        
        else if(strcmp(w[0], "Cancel") == 0)
        {
            switch(N)
            {
                case 2:
                    svid = cancel_update(w);
                    sprintf(server_response, "1%s", server_query);
                    fprintf(stdout, "%s - Server Request Success\n", server_response);
                    break;
                default:
                    sprintf(server_response, "0%s", server_query);
            }
            VALGRIND_MONITOR_COMMAND("leak_check summary");
        }
        else
        {
            svid = 0;
        }
        if(svid == 0)
        {
            strcpy(server_response, "0");
        }
        else 
        {
            sprintf(server_response, "1%08lu", svid);
        }

        VALGRIND_MONITOR_COMMAND("leak_check summary");
        response_length = strlen(server_response) + 1;
        bxp_response(bxp_service, &send, server_response, response_length);
    }   

    free(server_query);
    free(server_response);
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

        //Queue to store events that are ready to harvest
        const Queue *ready_q = Queue_create(NULL);

        // Queue to store repeat events
        const Queue *repeat_q = Queue_create(NULL);

        struct timeval *tval_ptr;

        Event *event_ptr;

        // Get events that are ready to harvest, add to ready_q
        pthread_mutex_lock(&lock);
        while(PQ->min(PQ, (void **)&tval_ptr, (void **)&event_ptr))
        {
            // if now >= tval_ptr
            if(compare((void *)tval_ptr, (void *)&now) <= 0)
            {
                PQ->removeMin(PQ, (void**)&tval_ptr, (void **)&event_ptr);
                // add to queue
                ready_q->enqueue(ready_q, event_ptr);
                free(tval_ptr);
            }
            else
            {
                break;
            }
            pthread_mutex_unlock(&lock);
            
            // process events in the queue
            while(ready_q->dequeue(ready_q, (void **)&event_ptr))
            {
                // if event not canceled, fire
                if(event_ptr->cancel == 0)
                {
                    fprintf(stdout, "Event fired: %lu|%s|%s|%u\n", event_ptr->clid, event_ptr->host, event_ptr->service, event_ptr->port);
                    if(event_ptr->repeat == 1 && event_ptr->num_repeats > 1)
                    {
                        event_ptr->num_repeats--;
                        repeat_q->enqueue(repeat_q, (void *)event_ptr);
                    }
                    else
                    {
                        map->remove(map, (void *)&event_ptr->id);
                        free(tval_ptr);
                        free(event_ptr->host);
                        free(event_ptr->service);
                        free(event_ptr);
                    }
                }
                else 
                {
                    map->remove(map, (void *)&event_ptr->id);
                    free(tval_ptr);
                    free(event_ptr->host);
                    free(event_ptr->service);
                    free(event_ptr);
                }
            }
        }
        pthread_mutex_lock(&lock);
        while(repeat_q->dequeue(repeat_q, (void**)&event_ptr))
        {
            later->tv_sec = now.tv_sec;
            later->tv_usec = now.tv_usec;
            while(later->tv_usec > 1000000)
            {
                later->tv_usec = later->tv_usec - 1000000;
                later->tv_sec++;
            }
            if(event_ptr->cancel == 0)
            {
                PQ->insert(PQ, (void *)later, (void *)event_ptr);
            }

        }
        pthread_mutex_unlock(&lock);
        ready_q->destroy(ready_q);
        repeat_q->destroy(repeat_q);
    }
    free(later);
    pthread_exit(NULL);
}


int main(UNUSED int argc, UNUSED char *argv[])
{
    PQ = HeapPrioQueue(compare, NULL, NULL);
    if(PQ == NULL)
    {
        fprintf(stderr, "Failure to create Priority Queue\n");
    }
    map = HashMap(128, 4.0, hashFxn, compare_map, NULL, NULL);
    if(map == NULL)
    {
        fprintf(stderr, "Failure to create Map\n");

    }
    pthread_t receiver_thread, timer_thread;
    pthread_mutex_init(&lock, NULL);

    pthread_create(&timer_thread, NULL, timer, NULL);
    pthread_create(&receiver_thread, NULL, dtsv3, NULL);

    pthread_join(receiver_thread, NULL);
    pthread_join(timer_thread, NULL);
    PQ->destroy(PQ);
    map->destroy(map);
    return 0;
}