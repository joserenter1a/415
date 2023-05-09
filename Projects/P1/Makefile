CFLAGS=-W -Wall -I/usr/local/include -g
LDFLAGS=-L/usr/local/lib -g
PROGRAMS=uspsv1 uspsv2 uspsv3 uspsv4  
OBJECTS=uspsv1.o uspsv2.o uspsv3.o uspsv4.o p1fxns.o
LIBRARIES = -lADTs
all: $(PROGRAMS)

uspsv1: uspsv1.o p1fxns.o
	gcc $(LDFLAGS) -o $@ $^ $(LIBRARIES)

uspsv2: uspsv2.o p1fxns.o
	gcc $(LDFLAGS) -o $@ $^ $(LIBRARIES)

uspsv3: uspsv3.o p1fxns.o
	gcc $(LDFLAGS) -o $@ $^ $(LIBRARIES)

uspsv4: uspsv4.o p1fxns.o
	gcc $(LDFLAGS) -o $@ $^ $(LIBRARIES)

uspsv1.o: uspsv1.c p1fxns.c
uspsv2.o: uspsv2.c p1fxns.c
uspsv3.o: uspsv3.c p1fxns.c
uspsv4.o: uspsv4.c p1fxns.c


clean:
	rm -f $(PROGRAMS) *.o
