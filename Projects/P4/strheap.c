/*
Jose Renteria
jrenter3 951742079
CIS415 Project 4

This is my own work
*/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "strheap.h"
#include "ADTs/hashcskmap.h"

#define UNUSED __attribute__((unused))


typedef struct strnode
{
    char *str;
    unsigned int reference_count;
} StrNode; 

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// define hashcsk map to be used for bucket 
// hashing and storing str references

const CSKMap *strheap = NULL;

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
        strheap = HashCSKMap(1024, 5.0, free);
    }
    // check if the string already existing in the hashmap
    StrNode *existing;
    
    if(strheap -> get(strheap, str, (void **) &existing))
    {
        pthread_mutex_lock(&mutex);
        existing->reference_count++;
        pthread_mutex_unlock(&mutex);
        return existing->str;
    }
    else
    {
        StrNode* new = strnode_helper(str);
        if(new == NULL)
        {
            return NULL;
        }
        strheap->put(strheap, str, new);
        return new->str;
    }
    atexit(cleanup);

}

bool str_free(char *str)
{
    pthread_mutex_lock(&mutex);

    if(strheap != NULL)
    {        
        StrNode* existing;
        if(strheap -> get(strheap, str, (void **) &existing))
        {
            existing->reference_count --;
            // pthread_mutex_unlock(&mutex);
 
            if(existing->reference_count == 0)
            {
                strheap->remove(strheap, str);
                free(existing->str);
                // free(existing);

            }
        }
    }
    pthread_mutex_unlock(&mutex);

    atexit(cleanup);
    return false;

}

