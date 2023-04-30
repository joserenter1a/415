CFLAGS=-W -Wall -I/usr/local/include -g
LDFLAGS=-L/usr/local/lib -g
PROGRAMS=uspsv2 p1fxns  
OBJECTS=uspsv2.o p1fxns.o
LIBRARIES = -lADTs
all: uspsv2

uspsv2: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

uspsv2.o: p1fxns.c p1fxns.h


clean:
	rm -f $(PROGRAMS) *.o
