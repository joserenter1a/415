/*
Jose Renteria
951742079

This is my own work, apart from examples taken from the handout.
I discussed some of the server response logic with Chance Curran.


*/

#include "bxp/bxp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

// Macros
#define UNUSED __attribute__((unused))
#define PORT 19999
#define SERVICE "DTS"

// Based on the 5.1.3 example in the P3 handout
void *dtsv1()
{
    // Initialize BXP Endpoint and Service
    BXPEndpoint send;
    BXPService bxp_service;

    // malloc for our query and response
    char *server_query = (char *)malloc(BUFSIZ);
    char *server_response = (char *)malloc(BUFSIZ + 1);

    // the lengths of our query and response
    unsigned query_length, response_length;
    
    // Initialize our BXP protocol, bound to our local Port
    assert(bxp_init(PORT, 1));

    // Offer our BXP Service
    bxp_service = bxp_offer(SERVICE);

    int K = 0;
    // If the service is null, exit and free
    if(bxp_service == NULL)
    {
        fprintf(stderr, "Failure to initiailize service\n");
        free(server_response);
        free(server_query);
        exit(EXIT_FAILURE);
    }
    do
    {
        K++;
        if(K == 1)
        {
            fprintf(stdout, "%s - Server Connection Established\n", server_response);
        }
        else
        {
            // parse our query string
            server_query[query_length] = '\0';
            char command[1000];
            strcpy(command, server_query);
            sprintf(server_response, "1%s", command);
            fprintf(stdout, "%s - Server Request Success\n", server_response);
            response_length = strlen(server_response) + 1;
            bxp_response(bxp_service, &send, server_response, response_length);
        }
}   
    while((query_length = bxp_query(bxp_service, &send, server_query, BUFSIZ)) > 0);

    free(server_query);
    free(server_response);
    pthread_exit(NULL);
}

int main(UNUSED int argc, UNUSED char *argv[])
{
    // Create thread for our receiver
    pthread_t receiver_thread;

    // Ensure correct initialization of thread
    if (pthread_create(&receiver_thread, NULL, dtsv1, NULL))
    {
        fprintf(stderr, "Failed to create thread\n");
        return EXIT_FAILURE;
    }

    // Wait for thread to finish
    pthread_join(receiver_thread, NULL);
    return EXIT_SUCCESS;
}