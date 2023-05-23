#include "ADTs/arrayqueue.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define UNUSED __attribute__((unused))

typedef struct {
    bool cancelled;
    char *name;
} Item;

const Queue *workQ = NULL;
pthread_mutex_t lockQ = PTHREAD_MUTEX_INITIALIZER;
long active = 0;

void freeItem(Item *item) {
    free(item->name);
    free(item);
}

/*
 * worker thread - wakes up every 1000 ms to retrieve item from workQ
 *
 * if item->cancelled is true, returns item storage to heap
 * otherwise, prints item->name and enqueues the item on workQ
 */
void *worker(UNUSED void *args) {
    Item *item;

    while (active > 0) {
        bool status;
        usleep(1000000);		/* sleep for 1000 ms */
	pthread_mutex_lock(&lockQ);
	status = workQ->dequeue(workQ, ADT_ADDRESS(&item));
	if (status) {
            if (item->cancelled) {
                printf("item '%s' CANCELLED\n", item->name);
                freeItem(item);
		active--;
	    } else {
                printf("item '%s' processed\n", item->name);
		workQ->enqueue(workQ, ADT_VALUE(item));
	    }
	}
	pthread_mutex_unlock(&lockQ);
    }
    return NULL;
}

#define DEFAULT_NUMBER_OF_ITEMS 100
#define MAX_NUMBER_OF_ITEMS 1000
int main(int argc, char *argv[]) {
    Item *array[MAX_NUMBER_OF_ITEMS];
    long i, N = DEFAULT_NUMBER_OF_ITEMS;;
    pthread_t workThread;

    if (argc == 2) {
        sscanf(argv[1], "%ld", &N);
	if (N > MAX_NUMBER_OF_ITEMS)
            N = MAX_NUMBER_OF_ITEMS;
    }
    workQ = ArrayQueue(0L, doNothing);
    for (i = 0; i < N; i++) {
        char buf[20];
        Item *item = (Item *)malloc(sizeof(Item));
	sprintf(buf, "Item%03ld", i);
	item->name = strdup(buf);
	item->cancelled = false;
	array[i] = item;
	workQ->enqueue(workQ, ADT_VALUE(item));
	active++;
    }
/*
 * the work queue is now created with `N' items
 * start the worker thread to dequeue, print, and enqueue
 */
    pthread_create(&workThread, NULL, worker, NULL);
/*
 * start randomly selecting items in the Queue and cancelling them
 */
    while (active > 0) {
        long index = random() % N;	/* select an item at random */
        usleep(500000);		/* sleep 200 ms */
	pthread_mutex_lock(&lockQ);
	if (array[index] != NULL) {
            array[index]->cancelled = true;
	    array[index] = NULL;
	}
	pthread_mutex_unlock(&lockQ);
    }
    pthread_join(workThread, NULL);
    return EXIT_SUCCESS;
}
