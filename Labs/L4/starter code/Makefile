CFLAGS=-W -Wall
PROGRAMS=timeit chldhandler doticks cpubound

all: $(PROGRAMS)

timeit: timeit.o
	gcc -o $@ $^

chldhandler: chldhandler.o
	gcc -o $@ $^

doticks: doticks.o
	gcc -o $@ $^

cpubound: cpubound.o
	gcc -o $@ $^

timeit.o: timeit.c
chldhandler.o: chldhandler.c
doticks.o: doticks.c
cpubound.o: cpubound.c

clean:
	rm -f $(PROGRAMS) *.o
