#include "p1fxns.h"
#include <stdlib.h>
#define UNUSED __attribute__((unused))

int main(UNUSED int argc, UNUSED char *argv[]) {
	char buf[4096];

	while (p1getline(0, buf, sizeof buf) != 0)
		p1putstr(1, buf);
	return EXIT_SUCCESS;
}
