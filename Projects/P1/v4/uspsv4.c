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
#define SWITCHES 12

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
volatile int context = SWITCHES;

pid_t curr_pid;
const Queue *readyQueue = NULL;
PCB *curr = NULL;
int q_ticks;

static long p1atol(char *s)
{ 
    long a;
    for(a= 0L; *s >= '0' && *s <= '9'; s++)
    {
        a = 10 * a + (long)(*s - '0');
    }
    return a;
}

static void p1ltoa(long num, char *buf)
{
    char temp[25];
    long i, N, sign;
    static char ints[] = "0123456789";

    if (num == 0L)
    {
        temp[0] = '0';
        i = 1;
    } 
    else
    {
        if ((N = num) < 0L)
        {
            sign = 1; // is negative
            N *= -1; // negate N
        }
        else 
        {
            sign = 0;
        }
        for(i = 0; N != 0L; i++)
        {
            temp[i] = ints[N % 10];
            N /= 10;
        }
        if (sign == 1) // if negative
        {
            temp[i] = '-';
            i ++;
        }
    }
    while(--i >= 0)
    {
        *buf++ = temp[i];
    }
    *buf = '\0';
}




//collapses a large number string into readable text, 1000 to 1[K/M/B] etc.
static void collapse_num(char *num) {

    int len = p1strlen(num);
    if (len >= 16) {
        // collapse to quadrillion 
        num[len - 15] = 'Q';
        num[len - 14] = '\0';
    } else if (len >= 13) {
        // collapse to trillion 
        num[len - 12] = 'T';
        num[len - 11] = '\0';
    } else if (len >= 10) {
        // collapse to billion 
        num[len - 9] = 'B';
        num[len - 8] = '\0';
    } else if (len >= 7) {
        // collapse to million
        num[len - 6] = 'M';
        num[len - 5] = '\0';
    } else if (len >= 4) {
        // collapse to thousand 
        num[len - 3] = 'K';
        num[len - 2] = '\0';
    }
}


// collapse string specifying byte size into readable text abbrev.
static void collapse_bytes(char *bytes) {

    int len = p1strlen(bytes);
    if (len >= 13) {
        // collapse to TB 
        bytes[len - 12] = ' ';
        bytes[len - 11] = 'T';
        bytes[len - 10] = 'B';
        bytes[len - 9] = '\0';
    } else if (len >= 10) {
        // collapse to GB 
        bytes[len - 9] = ' ';
        bytes[len - 8] = 'G';
        bytes[len - 7] = 'B';
        bytes[len - 6] = '\0';
    } else if (len >= 7) {
        // collapse to megabytes 
        bytes[len - 6] = ' ';
        bytes[len - 5] = 'M';
        bytes[len - 4] = 'B';
        bytes[len - 3] = '\0';
    } else if (len >= 4) {
        // collapse to kilobytes 
        bytes[len - 3] = ' ';
        bytes[len - 2] = 'K';
        bytes[len - 1] = 'B';
        bytes[len - 0] = '\0';
    }
}

static void convert_ticks(char *buff) {
    // helper function to convert ticks into seconds

    unsigned long ticks = p1atol(buff);
    unsigned long ticks_per_sec = sysconf(_SC_CLK_TCK);
    unsigned long sec = (ticks / ticks_per_sec);
    // converts back to a string, and copies into buffer
    p1ltoa(sec, buff);
    collapse_num(buff);
}

static void convert_pages(char *buff) {

    // helper function to convert our pages into bytes
    unsigned long pages = p1atoi(buff);
    unsigned long bytes_per_page = sysconf(_SC_PAGESIZE);
    unsigned long bytes = (pages * bytes_per_page);
    // converts back to a string, and copies into buffer

    p1ltoa(bytes, buff);
    collapse_bytes(buff);
}


#define DELIMITER 20L // delimit the header by 20 lines

static void display(pid_t pid) 
{
    static unsigned long iter = 0;
    int i, index;
    int cmd_fd = -1, io_fd = -1, stat_fd = -1;
    char pid_str[32], syscr[32], syscw[32], stat[32], flts[32],
        usrtm[32], systm[32], vmsz[32], rssz[32], cmd[2048];
    char res[4096], buffer[4096], word[1024];
    char *res_ptr = NULL;

    // access the /proc/<pid>/cmdline
    p1itoa((int)(pid), pid_str);
    p1strcpy(buffer, "/proc/");
    p1strcat(buffer, pid_str);
    p1strcat(buffer, "/cmdline");
    if ((cmd_fd = open(buffer, O_RDONLY)) < 0)
        goto close_files;

    // stores our command into a buffer
    p1getline(cmd_fd, cmd, sizeof(cmd));
    i = 0;
    while (1) {
        if (cmd[i] == '\0') {
            i++;
            // extracts cmdline arguments and replaces terminating char with white space
            if (cmd[i] == '\0')
                break;
            cmd[--i] = ' ';
        }
        i++;
    }
    close(cmd_fd);
    cmd_fd = -1;

    // access the proc/<pid>/io 
    p1strcpy(buffer, "/proc/");
    p1strcat(buffer, pid_str);
    p1strcat(buffer, "/io");
    if ((io_fd = open(buffer, O_RDONLY)) < 0)
        goto close_files;
    i = 1;

    // we want to extract the system read and write calls
    while (p1getline(io_fd, buffer, sizeof(buffer))) {
        index = 0;
        index = p1getword(buffer, index, word);
        if (i == 3) {
            // system read calls on 3rd line
            index = p1getword(buffer, index, syscr);
            syscr[p1strlen(syscr)-1] = '\0';
            collapse_num(syscr);
        }
        if (i == 4) {
            // system write calls on 4th line
            index = p1getword(buffer, index, syscw);
            syscw[p1strlen(syscw)-1] = '\0';
            collapse_num(syscw);
        }
        i++;
    }
    close(io_fd);
    io_fd = -1;

    // access /proc/<pid>/stat
    p1strcpy(buffer, "/proc/");
    p1strcat(buffer, pid_str);
    p1strcat(buffer, "/stat");
    if ((stat_fd = open(buffer, O_RDONLY)) < 0)
        goto close_files;

    // all info on one line
    p1getline(stat_fd, buffer, sizeof(buffer));
    i = 1;
    index = 0;
    while ((index = p1getword(buffer, index, word)) != -1) {
        switch (i) {
            // status of process
            case 3:
                p1strcpy(stat, word);
                break;
            // gets number of major faults in our process
            case 12:
                p1strcpy(flts, word);
                collapse_num(flts);
                break;
            // displays how many ticks/time spent in user mode
            case 14:
                p1strcpy(usrtm, word);
                convert_ticks(usrtm);
                break;
            // displays how many ticks/time spent in kernel mode
            case 15:
                p1strcpy(systm, word);
                convert_ticks(systm);
                break;
            // displays virtual memory size
            case 23:
                p1strcpy(vmsz, word);
                collapse_bytes(vmsz);
                break;
            // displays size of resident set 
            case 24:
                p1strcpy(rssz, word);
                convert_pages(rssz);
                break;
            // we don't care about the rest
            default:
                break;
        }
        i++;
    }
    close(stat_fd);
    stat_fd = -1;

    // display the header when needed
    if (!iter)
        p1putstr(STDOUT_FILENO, "PID      SysCR   SysCW   State  Flts    UsrTm   SysTm   VMSz    RSSz    Cmd\n");
    // keep tally of how many lines printed
    if (++(iter) >= DELIMITER)
        iter = 0;
        
    // packs into one single string for easier print and format

    res_ptr = res;
    res_ptr = p1strpack(pid_str, 9, ' ', res_ptr);
    res_ptr = p1strpack(syscr, 8, ' ', res_ptr);
    res_ptr = p1strpack(syscw, 8, ' ', res_ptr);
    res_ptr = p1strpack(stat, 7, ' ', res_ptr);
    res_ptr = p1strpack(flts, 8, ' ', res_ptr);
    res_ptr = p1strpack(usrtm, 8, ' ', res_ptr);
    res_ptr = p1strpack(systm, 8, ' ', res_ptr);
    res_ptr = p1strpack(vmsz, 8, ' ', res_ptr);
    res_ptr = p1strpack(rssz, 8, ' ', res_ptr);
    res_ptr = p1strpack(cmd, 0, ' ', res_ptr);

    // print out process info
    p1putstr(STDOUT_FILENO, res);
    p1putstr(STDOUT_FILENO, "\n");
    return;

close_files:
// close any open files and return
    if (cmd_fd != -1)
        close(cmd_fd);
    if (io_fd != -1)
        close(io_fd);
    if (stat_fd != -1)
        close(stat_fd);
    return;
}



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
            display(curr->pid);
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
    UNUSED struct timespec ms20 = {0, 20000000}; // 20 ms
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