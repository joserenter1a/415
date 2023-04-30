#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#define UNUSED __attribute__((unused))

#define NUMBER 5
char *args[] = { "./cpubound", "-s", "5", NULL};
volatile int nprocesses = 0;

static void onusr2(UNUSED int sig)
{

}
static void onchld(UNUSED int sig)
{
	pid_t pid;
	int status;
	while((pid = waitpid(-1, &status, WNOHANG))> 0)
	{
		if(WIFEXITED(status) || WIFSIGNALED(status))
		{
			nprocesses --;
			kill(getpid(), SIGUSR2);
		}
	}
}

int main(UNUSED int argc, UNUSED char *argv[]) {
	int i;
	signal(SIGCHLD, onchld);
	signal(SIGUSR2, onusr2);

	for(i = 0; i < NUMBER; i++)
	{
		int pid = fork();
		switch(pid)
		{
			case -1: fprintf(stderr, "Parent: fork() failed\n");
			goto wait_for_children;
			case 0: execvp(args[0], args);
				fprintf(stderr, "Child: execvp() failed\n");
				exit(EXIT_FAILURE);
			default: nprocesses++;

		}
	}
	wait_for_children:
		while(nprocesses > 0)
			pause();
			
	return EXIT_SUCCESS;
}
