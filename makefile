CC = gcc
CFLAGS = -g
LDFLAGS += -lwiringPi

all: PiMonitor 

PiMonitor: PiMonitor.o 
	$(CC) $(LDFLAGS) -o $@ $^

