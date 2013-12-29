CC = gcc
CFLAGS = -g

all: PiMonitor 

PiMonitor: PiMonitor.o BroadcastCommon.o 
	$(CC) $(LDFLAGS) -o $@ $^

BroadcastCommon.o: BroadcastCommon.c 
	$(CC) $(LDFLAGS) -c -o $@ $^

