#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    long long i;
    int minutes = 0, seconds = 0, j, opt;
    char name[128];

    opterr = 0;
    while((opt=getopt(argc, argv, "m:s:n:")) != -1)
    {
        switch(opt)
        {
            case 'm': minutes = atoi(optarg); break;
            case 's': seconds = atoi(optarg); break;
            case 'n': strcpy(name, optarg); break;
        default:
            fprintf(stderr, "illegal flag: '-%c'\n", optopt);
            return EXIT_FAILURE;
        }
    }
    seconds += 60 * minutes;
    for(j = 0; j < seconds; j++)
    {
        for(i = 0; i < 10000; i++)
        {
            ;
        }
    }
    return EXIT_SUCCESS;
}
