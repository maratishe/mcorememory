CC=gcc
FLAGS=-w -Wall
HEADERS=-I/usr/local/pfring/kernel -I/usr/local/pfring/userland/lib
LIBDIRS=-L/usr/local/pfring/userland/lib -L/usr/local/pfring/userland/libpcap-1.1.1-ring
LIBS=-lstdc++ -lc -lm -lpfring -lpcap -lpthread -lrt
all: 
	$(CC) $(FLAGS) $(HEADERS) $(LIBDIRS) -o mcore.manager mcore.manager.c $(LIBS)
	$(CC) $(FLAGS) $(HEADERS) $(LIBDIRS) -o mcore.task mcore.task.c $(LIBS)
	
	