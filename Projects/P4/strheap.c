/*
Jose Renteria
jrenter3 951742079
CIS415 Project 4

This is my own work, except for some debugging help with my free function in Adam's office hours.
*/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "strheap.h"
#include "ADTs/hashcskmap.h"

#define UNUSED __attribute__((unused))

// define hashcsk map to be used for bucket 
// hashing and storing str references

const CSKMap *strheap = NULL;

// mutex lock to make our str malloc and free thread safe
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// struct that stores the reference count and str itself
typedef struct strnode
{
    char *str;
    unsigned int reference_count;
} StrNode; 


// cleanup function to destroy the map
void cleanup()
{
    if(strheap != NULL)
    {                
        strheap->clear(strheap);
        strheap->destroy(strheap);
        strheap = NULL;
    }
}


// abstracted helper function that will
// return a node after mallocing, incrementing
// ref count, and initialising mutex
StrNode *strnode_helper(const char *str)
{
    StrNode *node = (StrNode*)malloc(sizeof(StrNode));
    node->str = strdup(str);
    node->reference_count = 1;
    // from
    if(node->str == NULL)
    {
        fprintf(stderr, "str_malloc() failure\n");
        abort();
    }

    return node;
}

// str malloc function that will call our helper 
// and create our hashCSKmap
char *str_malloc(char* str)
{       
    if(strheap == NULL)
    {
        strheap = HashCSKMap(1024, 5.0, doNothing);
    }
    // check if the string already existing in the hashmap
    StrNode *existing;
    if(strheap -> get(strheap, str, (void **) &existing))
    {
        // lock it and increment ref count by 1
        pthread_mutex_lock(&mutex);
        existing->reference_count++;
        pthread_mutex_unlock(&mutex);
        // return the ptr to the str within our existing node struct
        return existing->str;
    }
    else
    {
        // malloc and strdup a new node
        StrNode* new = strnode_helper(str);
        if(new == NULL)
        {
            return NULL;
        }
        // put it onto our map
        strheap->put(strheap, str, new);
        // return the ptr to the str within our new node struct
        return new->str;
    }
    // cleanup
    atexit(cleanup);

}

bool str_free(char *str)
{
    // lock before we try to access the heap for thread safety
    pthread_mutex_lock(&mutex);
    if(strheap != NULL)
    {        
        // if the node already exists in our heap
        StrNode* existing;
        if(strheap -> get(strheap, str, (void **) &existing))
        {
            // decrement the reference count
            existing->reference_count --;
            // once the count reaches zero, we can remove and free 
            if(existing->reference_count == 0)
            {
                strheap->remove(strheap, str);
                free(existing->str);
                free(existing);

            }
        }
    }
    // unlock
    pthread_mutex_unlock(&mutex);
    // cleanup
    atexit(cleanup);
    return false;

}