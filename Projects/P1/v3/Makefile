CFLAGS=-W -Wall -I/usr/local/include -g
LDFLAGS=-L/usr/local/lib -g
PROGRAMS=uspsv3 p1fxns  
OBJECTS=uspsv3.o p1fxns.o
LIBRARIES = -lADTs
all: uspsv3

uspsv3: $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^

uspsv3.o: p1fxns.c p1fxns.h


clean:
	rm -f $(PROGRAMS) *.o
