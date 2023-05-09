/*
Jose Renteria
jrenter3 951740279
CS415 Project 1

This is my work, apart from instances of code pulled from Ch8 of SYSC
i.e. the usage of signal handlers. As well as the functions provided
in p1fxns.h and also some code from Lab.
I also pulled from a getopt tutorial from 212.


This version has to get the environment variable USPS_QUANTUM_MSEC, process command
line args, open the workload (if specified), read each command from stdin/workload,
fork a child process and exec the command in that child process, and then wait for
each child process to terminate
*/

#include "p1fxns.h"
#include "ADTs/queue.h"
#include <unistd.h> // access to getopt(), opterr, optind, optarg, optop, _exit()
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h> // access to signal(), kill, and SIG's
#include <time.h> // timespec, and nanosleep()
#include <sys/time.h>
#include <stdbool.h>

#define MIN_Q 20
#define MAX_Q 1000
#define TICK_FREQ 20
#define MAX_BUFSIZ 4096
#define MAX_PCBS 512
#define MAX_ARGS 128
#define UNUSED __attribute__ ((unused))

typedef struct pcb 
{
     pid_t pid; 
     int ticks_remaining;
     bool alive;
     bool send_usr1_signal;
} PCB;

// define our global vars

PCB pcb_arr[MAX_PCBS]; // array of Process control blocks

int pcount = 0;
volatile int active_procs = 0;
volatile int vis_USR1 = 0;
pid_t curr;
const Queue *ready_queue = NULL;
PCB *current_process = NULL;
int q_ticks = 0;

//sigusr1 handler
static void sigusr1_handler(UNUSED int signal)
{
    vis_USR1++;
}

//sigusr 2 does nothing

static void sigusr2_handler(UNUSED int signal) {}

static int pid_search(pid_t pid)
{
    int i;
    for(i = 0; i < pcount; i++)
    {
        if((pcb_arr[i].pid) == pid)
        {
            return i;
        }
        
    }
    return -1;
}
// SIGCHILD SIGNAL handler
static void sigchld_handler(UNUSED int signal)
{
    pid_t pid;
    int flag;
    // waits for dead processes
    while((pid = waitpid(-1, &flag, WNOHANG)) > 0)
    {
        if(WIFSIGNALED(flag) || WIFEXITED(flag))
        {
            pcb_arr[pid_search(pid)].alive = false;
            active_procs--;
            kill(curr, SIGUSR2);
        }
    }

}
// SIGALARM handler, scheduler will be housed here
static void sigalarm_handler(UNUSED int signal)
{
    if(current_process != NULL)
    {
        if(current_process ->alive)
        {
            if(--(current_process->ticks_remaining) > 0) {return;}
             // the quantum hasn't expired yet
            (void) kill(current_process->pid, SIGSTOP);
            ready_queue->enqueue(ready_queue, ADT_VALUE(current_process));
        }
        current_process = NULL;
    }
    while(ready_queue->dequeue(ready_queue, ADT_ADDRESS(&current_process)))
    {
        if(! current_process->alive)
        {
            continue;
        }
        current_process->ticks_remaining = q_ticks;
        if(current_process->send_usr1_signal)
        {
            current_process->send_usr1_signal = false;
            (void) kill(current_process->pid, SIGUSR1);
        }
        else
        {
            (void) kill(current_process->pid, SIGCONT);
        }
        return;
    }
}
char *env = "USPS_QUANTUM_MSEC";
static void print_usg_exit(char *name)
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
    int n, opt, k; // k - iteration variable 
    struct timespec ms20 = {0, 20000000};
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
                     print_usg_exit(program_name);
        }
    }
    if((argc - optind) > 1)
    {
        print_usg_exit(program_name);
    }

    fd = argv[optind];

    if(Q == 0)
    {
        p1putstr(2, "Quantum not defined\n"); print_usg_exit(program_name);
    }

    if(fd != NULL)
    {
        if((wfd = open(fd, O_RDONLY)) == -1)
        {
            p1perror(2, fd); print_usg_exit(program_name);
        }
        else
        {
            wfd = 0; // stdin
        }
        
    }
    // USR1 Signal Handler
    if(signal(SIGUSR1, sigusr1_handler) == SIG_ERR)
    {
        p1perror(2, "Can't establish SIGUSR1 handler");
        if((wfd != 0))
        {
            close(wfd);
        }
        _exit(-1);
    }

    // USR2 Signal Handler
    if(signal(SIGUSR2, sigusr2_handler) == SIG_ERR)
    {
        p1perror(2, "Can't establish SIGUSR2 handler");
        if((wfd != 0))
        {
            close(wfd);
        }
        _exit(-1);
    }
    curr = getpid();
    // CHILD Signal Handler
    if(signal(SIGCHLD, sigchld_handler) == SIG_ERR)
    {
        p1perror(2, "Can't establish SIGCHLD handler");
        if((wfd != 0))
        {
            close(wfd);
        }
        _exit(-1);
    }

        // CHILD Signal Handler
    if(signal(SIGALRM, sigalarm_handler) == SIG_ERR)
    {
        p1perror(2, "Can't establish SIGALRM handler");
        if((wfd != 0))
        {
            close(wfd);
        }
        _exit(-1);
    }
// keeps quantum within our defined bounds
    if(Q < MIN_Q)
    {
        p1putstr(2, "Reassigning Quantum to min");
        Q = MIN_Q;
    }
    else if (Q > MAX_Q)
    {
        p1putstr(2, "Reassining Quantum to max");
        Q = MAX_Q;
    }
    Q = 20 * ((Q+ 1) / 20);
    q_ticks = Q / TICK_FREQ;

    if((ready_queue = Queue_create(doNothing)) == NULL)
    {
        p1perror(2, "Could not create readyqueue");
        if(wfd != 0)
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
        char word[MAX_BUFSIZ];
        char *args[MAX_ARGS];
        PCB *process = pcb_arr+pcount;
        
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
            p1perror(2, "Error forking new process"); break;
        } 

        else if(pid == 0) // child process
        {
            while(! vis_USR1)
                (void)nanosleep(&ms20, NULL);
            execvp(args[0], args);
            p1perror(2, "Child process: execvp error");
            p1putstr(2, buf);
            for(j = 0; args[j]!=NULL; j++)
            {
                free(args[j]);
            }
            ready_queue->destroy(ready_queue);
            _exit(-1); break;
        } 
        else // parent / default case
        {
            process->pid = pid;
            process->alive = true;
            process->send_usr1_signal = true;
            (void)ready_queue->enqueue(ready_queue, ADT_VALUE(process));
            for(j = 0; args[j] != NULL; j++)
            {
                free(args[j]);
            }
            break;
        }
        pcount++;
    }
    active_procs = pcount;

    it_val.it_value.tv_sec = TICK_FREQ / 1000;
    it_val.it_value.tv_usec = (TICK_FREQ * 1000) % 1000000;
    it_val.it_interval = it_val.it_value;
    if(setitimer(ITIMER_REAL, &it_val, NULL) == -1)
    {
        p1perror(2, "Could not call setitimer");
        for(int i = 0; i < pcount; i ++)
        {
            (void) kill(pcb_arr[i].pid, SIGKILL);
        }
        goto cleanup;
    }
    sigalarm_handler(SIGALRM);
    while(active_procs > 0)
    {

        pause();
    }
    cleanup:
        if(wfd != 0)
        {
            close(wfd);
        }
        ready_queue->destroy(ready_queue);
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


}