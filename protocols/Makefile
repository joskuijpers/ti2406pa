CFLAGS=-D_POSIX_SOURCE -m32
OBJ = simulator.o p2.o p3.o p4.o p5.o p6.o
CC=clang

all:	$(OBJ)
	$(CC) $(CFLAGS) -o protocol2 p2.o simulator.o
	$(CC) $(CFLAGS) -o protocol3 p3.o simulator.o
	$(CC) $(CFLAGS) -o protocol4 p4.o simulator.o
	$(CC) $(CFLAGS) -o protocol5 p5.o simulator.o
	$(CC) $(CFLAGS) -o protocol6 p6.o simulator.o

protocol2:	p2.o simulator.o
	$(CC) $(CFLAGS) -o protocol2 p2.o simulator.o

protocol3:	p3.o simulator.o
	$(CC) $(CFLAGS) -o protocol3 p3.o simulator.o

protocol4:	p4.o simulator.o
	$(CC) $(CFLAGS) -o protocol4 p4.o simulator.o

protocol5:	p5.o simulator.o
	$(CC) $(CFLAGS) -o protocol5 p5.o simulator.o

protocol6:	p6.o simulator.o
	$(CC) $(CFLAGS) -o protocol6 p6.o simulator.o

clean:
	rm -f *.o *.bak

simulator.o:	simulator.h protocol.h
p2.o:	protocol.h
p3.o:	protocol.h
p4.o:	protocol.h
p5.o:	protocol.h
p6.o:	protocol.h
