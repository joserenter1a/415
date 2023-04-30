#include <unistd.h>	/* defines _exit() */
#include <stdlib.h>	/* defines NULL, getenv() */
#include <time.h>	/* needed for struct timespec and nanosleep */
#include <sys/time.h>   /* needed for gettimeofday(), struct timeval */
                        /* and for setitimer(), struct itimerval */
#include <signal.h>     /* signal(), kill(), USR1, USR2, STOP, CONT */
#include <stdbool.h>	/* bool, true, false */
#include <stdio.h>

#define UNUSED __attribute__((unused))


int main(int argc, char **argv) {

    return EXIT_SUCCESS;
}
