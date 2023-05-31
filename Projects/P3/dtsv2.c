/*
Jose Renteria
951742079

This is my own work, apart from examples taken from the handout.


*/

#include "bxp/bxp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <valgrind/valgrind.h>

// Macros
#define UNUSED __attribute__((unused))
#define PORT 19999
#define SERVICE "DTS"

// This function was given in lab_7/cbserver.c
int extractWords(char *buf, char *sep, char *words[]) {
    int i;
    char *p;

    for (p = strtok(buf, sep), i = 0; p != NULL; p = strtok(NULL, sep), i++)
        words[i] = p;
    words[i] = NULL;
    return i;
}

// Built on top of dtsv1
void *dtsv2()
{
    // Initialize BXP Endpoint and Service
    BXPEndpoint send;
    BXPService bxp_service;

    char *server_query = (char *)malloc(BUFSIZ);
    char *server_response = (char *)malloc(BUFSIZ + 1);
    char *query_copy = (char *)malloc(sizeof(server_query));
    char *command, *parsed;

    unsigned query_length, response_length;

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

        parsed = strtok(command, "|");

        int N = 0;
        VALGRIND_MONITOR_COMMAND("leak_check summary");

        if(strcmp(parsed, "OneShot") == 0)
        {
            while(parsed != NULL)
            {
                // Increase count
                N++;
                // move to next arg
                parsed = strtok(NULL, "|");
            }
            switch(N)
            {
                case 7:
                    sprintf(server_response, "1%s", server_query);
                    fprintf(stdout, "%s - Server Request Success\n", server_response);
                    break;
                default:
                    sprintf(server_response, "0%s", server_query);
            }
            VALGRIND_MONITOR_COMMAND("leak_check summary");        
        }
        else if(strcmp(parsed, "Repeat") == 0)
        {
            while(parsed != NULL)
            {
                // Increase count
                N++;
                // move to next arg
                parsed = strtok(NULL, "|");
            }
            switch(N)
            {
                case 7:
                    sprintf(server_response, "1%s", server_query);
                    fprintf(stdout, "%s - Server Request Success\n", server_response);
                    break;
                default:
                    sprintf(server_response, "0%s", server_query);
            }
            VALGRIND_MONITOR_COMMAND("leak_check summary");
        }
        
        else if(strcmp(parsed, "Cancel") == 0)
        {
            while(parsed != NULL)
            {
                // Increase count
                N++;
                // move to next arg
                parsed = strtok(NULL, "|");
            }
            switch(N)
            {
                case 2:
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

int main(UNUSED int argc, UNUSED char *argv[])
{
    // Create thread for our receiver
    pthread_t receiver_thread;

    // Ensure correct initialization of thread
    if (pthread_create(&receiver_thread, NULL, dtsv2, NULL))
    {
        fprintf(stderr, "Failed to create thread\n");
        return EXIT_FAILURE;
    }

    // Wait for thread to finish
    pthread_join(receiver_thread, NULL);
    return EXIT_SUCCESS;
}