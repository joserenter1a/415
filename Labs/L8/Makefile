CFLAGS=-W -Wall -pedantic -I/usr/local/include
LDFLAGS=-L/usr/local/lib
PROGRAMS=lab8
LIBRARIES=-lADTs -lpthread

all: $(PROGRAMS)

lab8: lab8.o
	gcc -o $@ $(LDFLAGS) $^ $(LIBRARIES)

lab8.o: lab8.c

clean:
	rm -f $(PROGRAMS) *.o
