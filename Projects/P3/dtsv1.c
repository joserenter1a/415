#include "bxp/bxp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#define UNUSED __attribute__((unused))
#define PORT 19999
#define SERVICE "DTS"


#define USAGE "./dtsv1"

void *receiver();

int main(UNUSED int argc, UNUSED char *argv[])
{
    pthread_t receiver_thread;
    pthread_create(&receiver_thread, NULL, receiver, NULL);
    pthread_join(receiver_thread, NULL);
    return 0;
}

void *receiver()
{
    BXPEndpoint send;
    char *query = (char *)malloc(BUFSIZ);
    char *response = (char *)malloc(BUFSIZ);
    unsigned length;
    BXPService bxps;
    char *service;
    service = "DTS";


    assert(bxp_init(PORT, 1));
    bxps = bxp_offer(service);

    if(bxps == NULL)
    {
        fprintf(stderr, "Failure to initiailize service");
        free(query);
        free(response);
        exit(EXIT_FAILURE);
    }
    while((length = bxp_query(bxps, &send, query, BUFSIZ)) > 0)
    {
        query[length] = '\0';
        char command[1000];
        strcpy(command, query);
        sprintf(response, "1%s", command);
        bxp_response(bxps, &send, response, strlen(response) + 1);
    }
    free(query);
    free(response);
    pthread_exit(NULL);
}
