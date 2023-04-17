#include "hashset.h"  /* the .h file does NOT reside in /usr/local/include/ADTs */
#include <stdlib.h>
/* any other includes needed by your code */
#define UNUSED __attribute__((unused))

typedef struct node 
{
    struct node *next;
    void *entry;
} Node;
 
typedef struct s_data {
    /* definitions of the data members of self */
    void (*freeValue)(void *element); // a function used by destroy(), clear(), and remove() to return heap memoryu
    int (*cmpFxn)(void *first, void *second); // a function used to compare two elements in the set
    long capacity; // indication of initial capacity, 0L by default
    double loadFactor; // load factor to be maintained, that underlies the set implementation; 0.0 by default
    long (*hashFxn)(void *, long N); // produces a hash value for an element in the set
    void **buckets;
    long size;
    long next;
} SData;

/*
 * important - remove UNUSED attributed in signatures when you flesh out the
 * methods
 */

static void s_destroy(const Set *s) {
    /* implement the destroy() method */
    SData *sd = (SData *) s->self;
    free(sd->buckets);
    free(sd);
    free((void *) s);
}

static void s_clear(const Set *s) {
    /* implement the clear() method */
    SData *sd = (SData *) s->self;
    sd->capacity = 0L;
    sd->loadFactor = 0.0;
    sd->size = 0L;
}
static void resize(SData *sd)
{
    int N;
    long i, j;
    Node *q, **array;
    Node *p = (Node *)malloc(sizeof(Node));

    long trigger = sd->size / sd->capacity;
    
    if(trigger > sd->loadFactor)
    {
        N = 2 * sd->capacity;
        if (N == sd->capacity)
        {
            return;
        }
        array = (Node **)malloc(N * sizeof(Node *));
        if(array == NULL)
            return;
        for(j = 0; j < N; j ++)
        {
            array[j] = NULL;
            
        }
            /*
            now redistribute the entries into the new set of buckets    
            */
        for(i = 0; i < sd->capacity; i++)
        {
                for(p = sd->buckets[i]; p != NULL; p = q)
                {
                    q = p->next;
                    j = sd->hashFxn((p->entry), N);
                    p->next = array[j];
                    array[j] = p;   
                }
            free(sd->buckets);
            sd->buckets = array;
            sd->capacity = N;
        }
    }

}

static bool s_add(const Set *s,  void *member) {
    /* implement the add() method */
    SData *sd = (SData *)s->self;
    int placement = sd->hashFxn(member, sd->capacity);
    Node *p = (Node *)malloc(sizeof(Node));

    bool status = (p!= NULL);
    if(status)
    {
        size_t nbytes = 2 * (sd->capacity) * sizeof(void *);

        void **arr = (void **)realloc(sd->buckets, nbytes);
        if(arr != NULL)
        {
            status = true;
            sd->buckets = arr;
            sd->capacity *= 2;
        }
    }
    if(status)
    {
        if(sd->cmpFxn(sd->next, member) == 0){
            return false;
        }
        sd->buckets[sd->next++] = member;
    }
    return status;
}

static bool s_contains(UNUSED const Set *s, UNUSED void *member) {
    /* implement the contains() method */
    return 0;
}

static bool s_isEmpty(UNUSED const Set *s) {
    /* implement the isEmpty() method */
    SData *sd = (SData *)s->self;
    return (sd->size == 0L);

}

static bool s_remove(UNUSED const Set *s, UNUSED void *member) {
    /* implement the remove() method */
    return 0;
}

static long s_size(UNUSED const Set *s) {
    /* implement the size() method */
    SData *sd = (SData *)s->self;
    return sd->size;
}

static void **s_toArray(const Set *s, long *len) {
    /* implement the toArray() method */
    SData *sd = (SData *)s->self;

    void **tmp = NULL;
    if(s->size > 0L)
    {
        size_t nbytes = sd->size * sizeof(void *);
        tmp = (void **)malloc(nbytes);
        if(tmp != NULL)
        {
            long i, n = 0L;
            for(i = 0L; i < sd->capacity; i++)
            {
                Node *p = sd->buckets[i];
                while(p != NULL)
                {
                    tmp[n++] = &(p->entry);
                    p = p->next;
                }
            }
        }
    }
    if(tmp != NULL)
    {
        *len = sd->size;
    }
    return tmp;
}



static const Iterator *s_itCreate(UNUSED const Set *s) {
    /* implement the itCreate() method */
    SData *sd = (SData *)s->self;
    const Iterator *it = NULL;
    void **tmp = (void **)s_toArray(sd, sd->size);
    if(tmp != NULL)
    {
        it = Iterator_create(sd->size, tmp);
        if(it == NULL)
        {
            free(tmp);
        }
    }
    return it;
}

static Set template = {
    NULL, s_destroy, s_clear, s_add, s_contains, s_isEmpty, s_remove,
    s_size, s_toArray, s_itCreate
};

static const Set *newHashSet( void (*freeValue)(void*),  int (*cmpFxn)(void*, void*),
                    long capacity,  double loadFactor,
                    long (*hashFxn)(void *m, long N)
                  ) {
    /* construct a Set instance and return to the caller */
    Set *s = (Set *)malloc(sizeof(Set));
    if( s != NULL){
        SData *sd = (SData *)malloc(sizeof(SData));
        if(sd != NULL){
            long cap;
            void **array = NULL;
            cap = (capacity < 0) ? capacity : DEFAULT_SET_CAPACITY;
            array = (void **)malloc(cap * sizeof(void *));
            if(array != NULL){
                sd -> capacity = cap;
                sd -> loadFactor = 0L;
                sd -> freeValue = freeValue;
                sd -> cmpFxn = cmpFxn;
                sd -> hashFxn = hashFxn;
                *s = template;
                s->self = sd;
            } else {
                free(sd);
                free(s);
                s = NULL;
            }
        } else {
            free(s);
            s = NULL;
        }
    }
    return s;
}
const Set *HashSet( void (*freeValue)(void*),  int (*cmpFxn)(void*, void*),
                    long capacity,  double loadFactor,
                    long (*hashFxn)(void *m, long N)
                  ) {
                    return newHashSet(freeValue, cmpFxn, capacity, loadFactor, hashFxn);
                  }