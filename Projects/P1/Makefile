CFLAGS=-W -Wall -I/usr/local/include -g
LDFLAGS=-L/usr/local/lib -g
PROGRAMS=uspsv1 p1fxns  
OBJECTS=uspsv1.o p1fxns.o
LIBRARIES=-lADTs

all: uspsv1

uspsv1: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^ $(LIBRARIES)

uspsv1.o: p1fxns.c p1fxns.h


clean:
	rm -f $(PROGRAMS) *.o
