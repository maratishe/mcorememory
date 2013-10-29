#define _GNU_SOURCE
#include <signal.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/poll.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <net/ethernet.h>     /* the L2 protocols */
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <monetary.h>
#include <locale.h>
#include <sys/shm.h>
#include "pfring.h"

#define KEYVALUE 1000
typedef struct { // Flow
	int sip;
	int dip;
	int sport;
	int dport;
	int bytes;
	int packets;
	double startime;
	double lastime;
} Flow;
typedef struct { // DLLE : and Element of DLL
	void *prev;
	void *next;
	void *payload; 
} DLLE;
typedef struct { // DLL: Double Linked List
	int count;
	DLLE *head;
	DLLE *tail;
} DLL;
typedef struct { // MCoreTask : One Capture Task
	DLL *flows;
} MCoreTask;
typedef struct { // MCoreTraffic 
	 MCoreTask **cores;	// each core gets its own space 
} MCoreTraffic;
double getime() {
	struct timeval tv;
	gettimeofday( &tv, NULL);
	return ( double)( tv.tv_sec + ( ( double) tv.tv_usec) / 1000000.0);
}
int main( int argc, char **argv) {
	int status;
	MCoreTraffic *T;
	
	/*
	int file = shm_open( "/mcoretraffic", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	ftruncate( file, sizeof( MCoreTraffic));
	T = ( MCoreTraffic*)mmap( NULL, sizeof( MCoreTraffic), PROT_WRITE | PROT_READ, MAP_SHARED, file, 0);
	while ( 1) {
		sleep( 1);
		printf( "%d\n", ( int)getime());
	}
	*/
	
	status = shmget( KEYVALUE, sizeof( MCoreTraffic), S_IRUSR | S_IWUSR | IPC_CREAT | IPC_EXCL);
	if ( status == -1 && errno == EEXIST) status = shmget( KEYVALUE, sizeof( MCoreTraffic), S_IRUSR | S_IWUSR); 
	if ( status == -1) { perror( "shmget() failed!\n"); exit( 1); }
	T = ( MCoreTraffic*)shmat( status, NULL , 0);
	printf( "OK\n");
	
}

