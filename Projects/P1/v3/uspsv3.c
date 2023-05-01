/*
This version has to get the environment variable USPS_QUANTUM_MSEC, process command
line args, open the workload (if specified), read each command from stdin/workload,
fork a child process and exec the command in that child process, and then wait for
each child process to terminate
*/
#include "ADTs/queue.h" // will need to use a queue to implement round robin scheduling
#include "p1fxns.h"
#include <unistd.h> // access to getopt(), opterr, optind, optarg, optop, _exit()
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h> // access to signal(), kill, and SIG's
#include <time.h> // timespec, and nanosleep()


#define MAX_BUFSIZ 4096
#define MAX_PCBS 512
#define MAX_ARGS 128
#define UNUSED __attribute__ ((unused))

#define MIN_QUANTUM 20
#define MAX_QUANTUM 1000
#define MS_PER_TICK 20

typedef struct pcb 
{ 
    pid_t pid; 
    int ticks; // num of ticks before quantum expiry
    bool alive; // flag indicating status
    bool sendusr1; // flag to send usr1 signal
} PCB;

// define our global vars

PCB pcb_arr[MAX_PCBS]; // array of Process control blocks

int pcount = 0;
volatile int active_procs = 0;
volatile int USR1_seen = 0;
pid_t curr_pid;
const Queue *readyQueue = NULL;
PCB *curr = NULL;
int q_ticks;

// create a SIGUSR1 handler
static void onusr1(UNUSED int sig)
{
    USR1_seen++;
}

// create a SIGUSR2 handler
static void onusr2(UNUSED int sig)
{
    // does nothing
}
// search our pcb array to find the pid
static int pid_search(pid_t pid)
{
    for(int i = 0; i < pcount; i++)
    {
        return (pcb_arr[i].pid == pid) ? i : -1;
        // ternary statement, returns index if found, else -1
    }
    return -1;
}

// SIGCHILD handler
// Chapter 8 of SYSC
static void onchild(UNUSED int sig)
{
    pid_t pid;
    int flag;
    bool check = WIFEXITED(flag) || WIFSIGNALED(flag);

    // waits for dead processes
    // use a non blocking call
    // to not block child process from somewhere else in the program


    while((pid = waitpid(-1, &flag, WNOHANG)) > 0) 
    {
        if (check)
        {
            pcb_arr[pid_search(pid)].alive = false;
            active_procs--;
            kill(curr_pid, SIGUSR2);
        }
    }
}

// need to create a SIGALARM handler
static void onalarm(UNUSED int sig)
{
    // create our scheduler

    // first checks if there is a current process
    if (curr != NULL)
    {   
        // check if that process is alive still
        if(curr->alive)
        {
            // decrement ticks and run until it hits zero
            if(--(curr->ticks) > 0){return;}
            // sends a stop signal to the process and adds to our readyQueue
            (void) kill(curr->pid, SIGSTOP);
            // removes our process block from the head of the ready queue
            readyQueue->enqueue(readyQueue, ADT_VALUE(curr));
        }
        curr = NULL;
    }
    while(readyQueue->dequeue(readyQueue, ADT_ADDRESS(&curr)))
    {
        // when it finds another live process
        if(! curr -> alive){ continue; }
        curr->ticks = q_ticks;
        // initialize the ticks to our corresponding quantim ticks
        if(curr -> sendusr1) // if this is the first run through the process
        {
            curr->sendusr1 = false;
            (void) kill(curr->pid, SIGUSR1);
        }
        else
        {
            (void) kill(curr->pid, SIGCONT);
        }
        return;
    }
}

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
    int wfd = 0;
    int n, opt, k, j; // k - iteration variable 
    struct timespec ms20 = {0, 20000000}; // 20 ms
    struct itimerval it_val;

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

    if(Q == 0)
    {
        p1putstr(2, "Quantum not defined\n"); print_usage_exit(program_name);
    }

    if(fd != NULL)
    {
        if((wfd = open(fd, O_RDONLY)) == -1)
        {
            p1perror(2, fd); print_usage_exit(program_name);
        }
        else
        {
            wfd = 0; // stdin
        }
        
    }
    // USR1 Signal Handler
    if(signal(SIGUSR1, onusr1) == SIG_ERR)
    {
        p1putstr(2, "Could not establish SIGUSR1 handler");
        if((wfd != 0))
        {
            close(wfd);
        }
        _exit(-1);
    }

    if(signal(SIGUSR2, onusr2) == SIG_ERR)
    {
        p1putstr(2, "Could not establish SIGUSR2 handler\n");
        if(wfd != 0)
        {
            close(wfd);
        }
        _exit(-1);
    }
    curr_pid = getpid();

    if(signal(SIGCHLD, onchild) == SIG_ERR)
    {
        p1putstr(2, "Could not establish SIGCHLD handler\n");
        if(wfd != 0)
        {
            close(wfd);
        }
        _exit(-1);
    }

    if(signal(SIGALRM, onalarm) == SIG_ERR)
    {
        p1putstr(2, "Could not establish SIGALRM handler\n");
        if(wfd != 0)
        {
            close(wfd);
        }
        _exit(-1);
    }

    Q = 20 * ((Q + 1) / 20) ;
    q_ticks = Q / MS_PER_TICK;

    if ((readyQueue = Queue_create(doNothing)) == NULL)
    {
        p1putstr(2, "Could not create ready queue\n");
        if( wfd != 0)
        {
            close(wfd);
        }
        _exit(-1);
    }
    // once workload file is open and we have quantum value
    // read workload file, parse command and args
    // fork a new process to exec the comand, storing its PID in our PCB struct
    while( (n = p1getline(wfd, buf, MAX_BUFSIZ)) > 0)
    {
        pid_t pid;
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
        args[j] = NULL;        pid = fork();
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
        (void) wait(NULL);
        active_procs -= 1;
    }
    if( wfd != 0)
    {
        close(wfd);
    }

    it_val.it_value.tv_sec = MS_PER_TICK/1000;
    it_val.it_value.tv_usec = (MS_PER_TICK*1000) % 1000000;
    it_val.it_interval = it_val.it_value;
    if(setitimer(ITIMER_REAL, &it_val, NULL) == -1)
    {
        p1perror(2, "couldn't call setitimer()");
        for(j = 0; j < pcount; j++)
        {
            (void) kill(pcb_arr[j].pid, SIGKILL);
        }
        goto cleanup;
    }
    onalarm(SIGALRM);
    while(active_procs > 0)
    {
        pause();
    }
    cleanup:
        if(wfd != 0)
        {
            close(wfd);
        }
        readyQueue->destroy(readyQueue);
    // we have to send the USR1 signal to every current child process
    for(k = 0; k < pcount; k++)
    {
        (void)kill(pcb_arr[k].pid, SIGUSR1);
    }
    // we need to send a STOP signal to every current child process
    for(k = 0; k < pcount; k++)
    {
        (void)kill(pcb_arr[k].pid, SIGSTOP);
    }
    // we need to send a CONT signal to every current child process
    for(k = 0; k < pcount; k++)
    {
        (void)kill(pcb_arr[k].pid, SIGCONT);
    }
// these loops will send USR1, STOP, CONT signals to all children in that order
    while(active_procs > 0)
    {
        (void) wait(NULL); // waiting for child process to terminate
        active_procs --;
    }
    if(wfd != 0)
    {
        close(wfd); return EXIT_SUCCESS;
    }

}