/*
This version has to get the environment variable USPS_QUANTUM_MSEC, process command
line args, open the workload (if specified), read each command from stdin/workload,
fork a child process and exec the command in that child process, and then wait for
each child process to terminate
*/

#include "p1fxns.h"
#include <unistd.h> // access to getopt(), opterr, optind, optarg, optop, _exit()
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_BUFSIZ 4096
#define MAX_PCBS 512
#define MAX_ARGS 128

typedef struct pcb { pid_t pid; } PCB;

// define our global vars

PCB pcb_arr[MAX_PCBS]; // array of Process control blocks
int pcount = 0;
int active_procs = 0;
char *env = "USPS_QUANTUM_MSEC";
static void print_usage_exit(char *name)
{
    p1putstr(2, "usage: ");
    p1putstr(2, name);
    p1putstr(2, " [ -q <quantum_in_msec>] [workload_file]\n");
    _exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    char buf[MAX_BUFSIZ];
    int Q = 0;
    char *fd = NULL;
    char *sp = NULL;
    char *program_name = *argv;
    int workload = 0;
    int n, opt;

    // Process environment variable USPS_QUANTUM_MSEC & CLI args to uspsv

    if ((sp = getenv(env)) != NULL)
    {
        Q = p1atoi(sp);
    }
    opterr = 0;
    while((opt = getopt(argc, argv, "q:") )!= -1)
    {
        switch(opt)
        {
            case 'q': Q = p1atoi(optarg); break;
            default: p1putstr(2, "illegal option: -");
                     p1putstr(2, "\n");
                     p1putchr(2, optopt);
                     print_usage_exit(program_name);
        }
    }
    if((argc - optind) > 1)
    {
        print_usage_exit(program_name);
    }
    fd = argv[optind];
    if(fd != NULL)
    {
        if((workload = open(fd, O_RDONLY)) == -1)
        {
            p1perror(2, fd); print_usage_exit(program_name);
        }
        else
        {
            workload = 0; // stdin
        }
        
    }
    if(Q == 0)
    {
        p1putstr(2, "Quantum not defined\n"); print_usage_exit(program_name);
    }

    // once workload file is open and we have quantum value
    // read workload file, parse command and args
    // fork a new process to exec the comand, storing its PID in our PCB struct
    while( (n = p1getline(workload, buf, MAX_BUFSIZ)) > 0)
    {
        char word[MAX_BUFSIZ];
        char *args[MAX_ARGS];
        PCB *p = pcb_arr+pcount;
        
        int i = p1strlen(buf);
        if(buf[i - 1] == '\n')
        {
            buf[i - 1] = '\0';
        }
        i = 0;
        int j = 0;
        while( (i = p1getword(buf, i, word)) != -1)
        {
            args[j++] = p1strdup(word);
        }
        
        args[j] = NULL;
        pid_t pid = fork();

        if(pid == -1) // error with fork
        {
            p1putstr(2, "Error forking new process\n"); break;
        } 

        else if(pid == 0) // child process
        {
            execvp(args[0], args);
            p1putstr(2, "Child process: execvp error");
            p1putstr(2, buf);
            p1putstr(2, "\n");
            for(j = 0; args[j]!=NULL; j++)
            {
            free(args[j]);
            }
            _exit(-1); break;
        } 
        else // parent / default case
        {
            p->pid = pid;
            for(j = 0; args[j] != NULL; j++)
            {
            free(args[j]);
            }
        }
        pcount++;
    }
    active_procs = pcount;
    while(active_procs > 0)
    {
        (void) wait(NULL); // waiting for child process to terminate
        active_procs --;
    }
    if(workload != 0)
    {
        close(workload); return EXIT_SUCCESS;
    }

}