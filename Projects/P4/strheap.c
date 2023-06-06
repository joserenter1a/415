/*
Jose Renteria
jrenter3 951742079
CIS415 Project 4
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
    pthread_mutex_t mutex;
} StrNode; 

// define hashcsk map to be used for bucket 
// hashing and storing str references

const CSKMap *strheap;

// compare function for hashmap, from P3/dtsv
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

// abstracted helper function that will
// return a node after mallocing, incrementing
// ref count, and initialising mutex
StrNode *strnode_helper(const char *str)
{
    StrNode *node = (StrNode*)malloc(sizeof(StrNode));
    node->str = strdup(str);
    node->reference_count = 1;
    pthread_mutex_init(&node->mutex, NULL);
    return node;
}

// str malloc function that will call our helper 
// and create our hashCSKmap
char *str_malloc(char* str)
{        
    char**res;
    if(strheap == NULL)
    {
        fprintf(stderr, "Failure to create hashmap\n");
    }

    // check if the string already existing in the hashmap
    StrNode* existing = (StrNode*)strheap->get(strheap, (void *)str, NULL);
    if(existing != NULL)
    {
        pthread_mutex_lock(&existing->mutex);
        existing->reference_count++;
        pthread_mutex_unlock(&existing->mutex);
        *res = existing->str;
    }
    else
    {
        StrNode* new = strnode_helper(str);
        strheap->put(strheap, (void *)str, new);
        *res = new->str;
    }
}

bool str_free(char *str)
{
    bool ret;
    if(strheap == NULL)
    {
        return false;
    }

    StrNode* existing = (StrNode*)strheap->get(strheap, (void *)str, NULL);

    if(existing != NULL)
    {
        pthread_mutex_lock(&existing->mutex);
        existing->reference_count--;

        if(existing->reference_count == 0)
        {
            strheap->remove(strheap, (void *)str);
            ret = true;
            pthread_mutex_unlock(&existing->mutex);
            pthread_mutex_destroy(&existing->mutex);
            free(existing->str);
            free(existing);
        }
        else
        {
            pthread_mutex_unlock(&existing->mutex);
        }
    }
    return ret;
}

int main()
{
    // initialize the hashmap if not already
    strheap = HashCSKMap(100, 4.0, free);
    if(strheap != NULL)
    {
        printf("strheap initialized!");
    }
    
}