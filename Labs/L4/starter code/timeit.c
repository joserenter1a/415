#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#define UNUSED __attribute__((unused))

void fcopy(FILE *in, FILE *out)
{
	char buf[BUFSIZ];
	while(fgets(buf, sizeof(buf), in) != NULL)
	{
		fputs(buf, out);
	}
}

int main(UNUSED int argc, UNUSED char *argv[]) {
	struct timeval t1, t2;
	long long musecs;

	(void)gettimeofday(&t1, NULL);
	fcopy(stdin, stdout);
	(void)gettimeofday(&t2, NULL);
	musecs = 1000000 * (t2.tv_sec - t1.tv_sec) + (t2.tv_sec - t1.tv_sec);
	fprintf(stderr, "Elapsed Time: %Ld.%03d ms\n", musecs/1000, (int)(musecs % 1000));
	return EXIT_SUCCESS;
}
