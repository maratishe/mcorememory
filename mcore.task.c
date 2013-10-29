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

#define ALARM_SLEEP             				1
#define DEFAULT_SNAPLEN       			128
#define MAX_NUM_THREADS       			4
#define DEFAULT_DEVICE    				 "eth0"
#define NO_ZC_BUFFER_LEN     			9000


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
typedef struct { // DLLE
	void *prev;
	void *next;
	void *payload; 
} DLLE;
typedef struct { // DLL
	int count;
	DLLE *head;
	DLLE *tail;
} DLL;


pfring  *pd;
int verbose = 1; 
int num_threads;
unsigned long long numPkts[MAX_NUM_THREADS] = { 0 }, numBytes[MAX_NUM_THREADS] = { 0 };
int packetseqcount = 0; double packetseqtime = 0; int packetseqstart = -1; int packetseqgaps = 0; int packetseqlast = -1; // for seq monitoring
double lastimes[ 4];
int vidgaps[ 100000]; int vtimegaps[ 100000]; double vtime = -1; int vid; int vpos = 0;
u_int8_t wait_for_packet = 1, do_shutdown = 0, add_drop_rule = 0;
u_int8_t use_extended_pkt_header = 0, touch_payload = 0, enable_hw_timestamp = 0;
pfring_stat pfringStats;
pthread_rwlock_t statsLock;
static char hex[] = "0123456789ABCDEF"; unsigned int sip, dip, sport, dport, hashkey, myip, myport, dir, bytesin, bytesout;
typedef struct { // marat 20130504 for apnoms -- on-demand capture
	char prestuff[ 42];
	char key[ 10];
	char seqid[ 10];
	char buffer[ 10];
} MyPayload; 
typedef struct { // marat 20130829 for GDrive capture app
	char etherstuff[ 14];
	char prestuff[ 12];
	char sip[ 4];
	char dip[ 4];
	//char otherstuff[ 4];
	char sport[ 2];
	char dport[ 2];
} MyHeader;
int myindexin[ 256]; int myindexout[ 256]; // indexing for GDrive capture application
double startime; int mypos, key, seqid; FILE *out; MyPayload *mypayload; MyHeader *myheader; char *outroot;
double getime() {
	struct timeval tv;
	gettimeofday( &tv, NULL);
	return ( double)( tv.tv_sec + ( ( double) tv.tv_usec) / 1000000.0);
}
int crc24( unsigned char *bytes) { 	// returns hash digest of the array of bytes
	int L[] = { 
			0x00000000, 0x00d6a776, 0x00f64557, 0x0020e221, 0x00b78115, 0x00612663, 0x0041c442, 0x00976334,
			0x00340991, 0x00e2aee7, 0x00c24cc6, 0x0014ebb0, 0x00838884, 0x00552ff2, 0x0075cdd3, 0x00a36aa5,
			0x00681322, 0x00beb454, 0x009e5675, 0x0048f103, 0x00df9237, 0x00093541, 0x0029d760, 0x00ff7016,
			0x005c1ab3, 0x008abdc5, 0x00aa5fe4, 0x007cf892, 0x00eb9ba6, 0x003d3cd0, 0x001ddef1, 0x00cb7987,
			0x00d02644, 0x00068132, 0x00266313, 0x00f0c465, 0x0067a751, 0x00b10027, 0x0091e206, 0x00474570,
			0x00e42fd5, 0x003288a3, 0x00126a82, 0x00c4cdf4, 0x0053aec0, 0x008509b6, 0x00a5eb97, 0x00734ce1,
			0x00b83566, 0x006e9210, 0x004e7031, 0x0098d747, 0x000fb473, 0x00d91305, 0x00f9f124, 0x002f5652,
			0x008c3cf7, 0x005a9b81, 0x007a79a0, 0x00acded6, 0x003bbde2, 0x00ed1a94, 0x00cdf8b5, 0x001b5fc3,
			0x00fb4733, 0x002de045, 0x000d0264, 0x00dba512, 0x004cc626, 0x009a6150, 0x00ba8371, 0x006c2407,
			0x00cf4ea2, 0x0019e9d4, 0x00390bf5, 0x00efac83, 0x0078cfb7, 0x00ae68c1, 0x008e8ae0, 0x00582d96,
			0x00935411, 0x0045f367, 0x00651146, 0x00b3b630, 0x0024d504, 0x00f27272, 0x00d29053, 0x00043725,
			0x00a75d80, 0x0071faf6, 0x005118d7, 0x0087bfa1, 0x0010dc95, 0x00c67be3, 0x00e699c2, 0x00303eb4,
			0x002b6177, 0x00fdc601, 0x00dd2420, 0x000b8356, 0x009ce062, 0x004a4714, 0x006aa535, 0x00bc0243,
			0x001f68e6, 0x00c9cf90, 0x00e92db1, 0x003f8ac7, 0x00a8e9f3, 0x007e4e85, 0x005eaca4, 0x00880bd2,
			0x00437255, 0x0095d523, 0x00b53702, 0x00639074, 0x00f4f340, 0x00225436, 0x0002b617, 0x00d41161,
			0x00777bc4, 0x00a1dcb2, 0x00813e93, 0x005799e5, 0x00c0fad1, 0x00165da7, 0x0036bf86, 0x00e018f0,
			0x00ad85dd, 0x007b22ab, 0x005bc08a, 0x008d67fc, 0x001a04c8, 0x00cca3be, 0x00ec419f, 0x003ae6e9,
			0x00998c4c, 0x004f2b3a, 0x006fc91b, 0x00b96e6d, 0x002e0d59, 0x00f8aa2f, 0x00d8480e, 0x000eef78,
			0x00c596ff, 0x00133189, 0x0033d3a8, 0x00e574de, 0x007217ea, 0x00a4b09c, 0x008452bd, 0x0052f5cb,
			0x00f19f6e, 0x00273818, 0x0007da39, 0x00d17d4f, 0x00461e7b, 0x0090b90d, 0x00b05b2c, 0x0066fc5a,
			0x007da399, 0x00ab04ef, 0x008be6ce, 0x005d41b8, 0x00ca228c, 0x001c85fa, 0x003c67db, 0x00eac0ad,
			0x0049aa08, 0x009f0d7e, 0x00bfef5f, 0x00694829, 0x00fe2b1d, 0x00288c6b, 0x00086e4a, 0x00dec93c,
			0x0015b0bb, 0x00c317cd, 0x00e3f5ec, 0x0035529a, 0x00a231ae, 0x007496d8, 0x005474f9, 0x0082d38f,
			0x0021b92a, 0x00f71e5c, 0x00d7fc7d, 0x00015b0b, 0x0096383f, 0x00409f49, 0x00607d68, 0x00b6da1e,
			0x0056c2ee, 0x00806598, 0x00a087b9, 0x007620cf, 0x00e143fb, 0x0037e48d, 0x001706ac, 0x00c1a1da,
			0x0062cb7f, 0x00b46c09, 0x00948e28, 0x0042295e, 0x00d54a6a, 0x0003ed1c, 0x00230f3d, 0x00f5a84b,
			0x003ed1cc, 0x00e876ba, 0x00c8949b, 0x001e33ed, 0x008950d9, 0x005ff7af, 0x007f158e, 0x00a9b2f8,
			0x000ad85d, 0x00dc7f2b, 0x00fc9d0a, 0x002a3a7c, 0x00bd5948, 0x006bfe3e, 0x004b1c1f, 0x009dbb69,
			0x0086e4aa, 0x005043dc, 0x0070a1fd, 0x00a6068b, 0x003165bf, 0x00e7c2c9, 0x00c720e8, 0x0011879e,
			0x00b2ed3b, 0x00644a4d, 0x0044a86c, 0x00920f1a, 0x00056c2e, 0x00d3cb58, 0x00f32979, 0x00258e0f,
			0x00eef788, 0x003850fe, 0x0018b2df, 0x00ce15a9, 0x0059769d, 0x008fd1eb, 0x00af33ca, 0x007994bc,
			0x00dafe19, 0x000c596f, 0x002cbb4e, 0x00fa1c38, 0x006d7f0c, 0x00bbd87a, 0x009b3a5b, 0x004d9d2d
	};
	int key = L[ 0]; int i;
	for ( i = 0; i < sizeof( bytes); i++) key = ( key >> 8) ^ L[ key ^ bytes[ i]]; 
	return key;
}
char* etheraddr_string(const u_char *ep, char *buf) {
  u_int i, j;
  char *cp;

  cp = buf;
  if((j = *ep >> 4) != 0)
    *cp++ = hex[j];
  else
    *cp++ = '0';

  *cp++ = hex[*ep++ & 0xf];

  for(i = 5; (int)--i >= 0;) {
    *cp++ = ':';
    if((j = *ep >> 4) != 0)
      *cp++ = hex[j];
    else
      *cp++ = '0';

    *cp++ = hex[*ep++ & 0xf];
  }

  *cp = '\0';
  return (buf);
}
void sigproc( int sig) {
	static int called = 0;
	printf( "Interrupt caught ... quitting\n");
	if ( called) return; else called = 1;
	do_shutdown = 1;
	//print_stats();
	pfring_breakloop( pd);
}
char buf1[ 128], buf2[ 64];	// used by packet
int myheaderparseip( unsigned char *ip) { return ( ip[ 0] << 24) | ( ip[ 1] << 16) | ( ip[ 2] << 8) | ip[ 3]; }
int myheaderparseport( unsigned char *port) { return ( port[ 0] << 8) | port[ 1]; }
void packet( const struct pfring_pkthdr *h, const u_char *p, const u_char *user_bytes) {
	long threadId = ( long)user_bytes;
	if( touch_payload) {
		volatile int __attribute__ ((unused)) i;
		i = p[12] + p[13];
	}
	struct ether_header *ehdr;
	int s, i;
	uint usec;
	uint nsec=0;
	if( h->ts.tv_sec == 0) {
		memset( ( void*)&h->extended_hdr.parsed_pkt, 0, sizeof( struct pkt_parsing_info));
		pfring_parse_pkt( ( u_char*)p, ( struct pfring_pkthdr*)h, 5, 1, 1);
	}
	s = ( h->ts.tv_sec + 0) % 86400;
	usec = h->ts.tv_usec;
	//printf( "%02d:%02d:%02d.%06u%03u ", s / 3600, (s % 3600) / 60, s % 60, usec, nsec);
	
	ehdr = (struct ether_header *)p;
	
	//printf(
	//	"[%s -> %s][eth_type=0x%04X][caplen=%d][len=%d] (use -m for details)\n",
	//	etheraddr_string(ehdr->ether_shost, buf1),
	//	etheraddr_string(ehdr->ether_dhost, buf2), 
	//	ntohs(ehdr->ether_type),
	//	h->caplen, h->len
	//);
	//numPkts[ threadId]++, numBytes[ threadId] += h->len + 24; // 8 Preamble + 4 CRC + 12 IFG
	
	
	double now = getime(); 
	
	
	// parse my header -- sip, dip, sport, dport     -- then hash
	myheader = ( MyHeader*)p;
	sip = myheaderparseip( myheader->sip); 
	sport = myheaderparseport( myheader->sport);
	//printf( "sip: %u.%u.%u.%u:%d", ( n >> 24) & 0xff, ( n >> 16) & 0xff, ( n >> 8) & 0xff, n & 0xff, n2); 
	dip = myheaderparseip( myheader->dip); 
	dport = myheaderparseport( myheader->dport);
 	dir = 0;
 	sprintf( buf1, "%u.%u.%u.%u", ( sip >> 24) & 0xff, ( sip >> 16) & 0xff, ( sip >> 8) & 0xff, sip & 0xff); if ( ! strcmp( buf1, "131.206.29.205")) dir = 1;
 	sprintf( buf1, "%u.%u.%u.%u", ( dip >> 24) & 0xff, ( dip >> 16) & 0xff, ( dip >> 8) & 0xff, dip & 0xff); if ( ! strcmp( buf1, "131.206.29.205")) dir = 2;
 	myip = 0; 
 	if ( dir == 1) { myip = dip; myport = dport; }  
 	if ( dir == 2) { myip = sip; myport = sport; } 
 	// hash using 5 bits for IP and 3 bits for ports
 	hashkey = 0;
 	if ( myip) hashkey = 0x00
 	| ( ( ( myip >> 31) & 0x01) << 7)
	| ( ( ( myip >> 28) & 0x01) << 6)
	| ( ( ( myip >> 24) & 0x01) << 5)
	| ( ( ( myip >> 18) & 0x01) << 4)
	| ( ( ( myip >> 13) & 0x01) << 3)
	| ( ( ( myport >> 15) & 0x01) << 2)
	| ( ( ( myport >> 10) & 0x01) << 1)
	| ( ( myport >> 4) & 0x01);
	if ( hashkey > 255) hashkey = 255;
	if ( dir == 1) myindexout[ hashkey] += h->len + 24;
	if ( dir == 2) myindexin[ hashkey] += h->len + 24;
	//printf( "dir:%d  myip:%d  hashkey:%d\n", dir, myip, hashkey);
 	if ( now - lastimes[ threadId] > 2.5) { 
		bytesin = 0; for ( i = 0; i < 256; i++) bytesin += myindexin[ i];
		bytesout = 0; for ( i = 0; i < 256; i++) bytesout += myindexout[ i];
 		sprintf( buf1, 	// packet and byte counts are in the filename
			"%s.%d.%d", 
			outroot, 
			(int)now, 
			( int)( 1000000.0 * ( now - ( int)now))
		);
		printf( "%s   IN:%d  OUT:%d\n", buf1, bytesin, bytesout);
		//printf( "Thread %d   time[%f]   %d packets\n", threadId, now - startime, numPkts[ threadId]);
		out = fopen( buf1, "w"); 
		for ( i = 0; i < 256; i++) { if ( ! myindexin[ i] && ! myindexout[ i]) continue; fprintf( out, "%d.%d.%d\n", i, myindexin[ i], myindexout[ i]); myindexin[ i] = 0; myindexout[ i] = 0; } 
		fclose( out);
		lastimes[ threadId] = now;
	}
	
}
int bind2core(u_int core_id) {
	cpu_set_t cpuset;
	int s;
	
	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);
	if((s = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) != 0) {
		fprintf(stderr, "Error while binding to core %u: errno=%i\n", core_id, s);
		return(-1);
	} else {
		return(0);
	}
	
}
void* packet_thread( void* _id) {
	long thread_id = (long)_id;
	//lastimes[ threadId] = getime();	// start time of this thread
	u_int numCPU = sysconf( _SC_NPROCESSORS_ONLN );
	u_char buffer[NO_ZC_BUFFER_LEN];
	u_char *buffer_p = buffer;
	u_long core_id = thread_id % numCPU;
	struct pfring_pkthdr hdr;
	//if(( num_threads > 1) && ( numCPU > 1)) {
	//	if(bind2core(core_id) == 0)
	//		printf("Set thread %lu on core %lu/%u\n", thread_id, core_id, numCPU);
	//}
	memset(&hdr, 0, sizeof(hdr));
	while ( 1) {
		int rc;
		u_int len;
		
		if(do_shutdown) break;
		
		if((rc = pfring_recv(pd, &buffer_p, NO_ZC_BUFFER_LEN, &hdr, wait_for_packet)) > 0) {
			if(do_shutdown) break;
			packet(&hdr, buffer, (u_char*)thread_id);
			#ifdef TEST_SEND
			buffer[0] = 0x99;
			buffer[1] = 0x98;
			buffer[2] = 0x97;
			pfring_send(pd, buffer, hdr.caplen);
			#endif
		} else {
			if(wait_for_packet == 0) sched_yield();
		} 
		if(0) {
			struct simple_stats {
				u_int64_t num_pkts, num_bytes;
			};
			struct simple_stats stats;
			
			len = sizeof(stats);
			rc = pfring_get_filtering_rule_stats(pd, 5, (char*)&stats, &len);
			if(rc < 0)
				fprintf(stderr, "pfring_get_filtering_rule_stats() failed [rc=%d]\n", rc);
			else {
				printf("[Pkts=%u][Bytes=%u]\n",
					(unsigned int)stats.num_pkts,
					(unsigned int)stats.num_bytes);
			}
		}
	}
	return( NULL);
}
int main( int argc, char **argv) {	// [1 number of threads][2 id]
	//mypayload = ( MyPayload*)malloc( sizeof( MyPayload));
	long i;
	for ( i = 0; i < 256; i++) { myindexin[ i] = 0; myindexout[ i] = 0; }  // for GDrive capture app
	char *device = NULL, c, buf[32], *reflector_device = NULL;
	u_char mac_address[6] = { 0 };
	int promisc, snaplen = DEFAULT_SNAPLEN, rc;
	u_int clusterId = 0;
	u_int32_t flags = 0;
	int bind_core = -1;
	packet_direction direction = rx_and_tx_direction;
  	u_int16_t watermark = 0, poll_duration = 0, 
  	cpu_percentage = 0, rehash_rss = 0;
  	char *bpfFilter = NULL;
  	
  	
  	// command line parameters
  	if ( argc != 3) { printf( "wrong command line: [1 num of threads][2 abs root of output]\n"); exit( 1); }
  	num_threads = atoi( argv[ 1]);
  	outroot = argv[ 2];
  	//sprintf( buf, "%s", argv[ 2]);
  	//out = fopen( buf, "w");
  	
  	
  	pfring_config( 50);	// cpu percentage
  	//flags |= PF_RING_REENTRANT; // if more than one core
  	//flags |= PF_RING_LONG_HEADER;	// extended packet header
 	flags = PF_RING_PROMISC; 	// promiscuous mode
  	//flags |= PF_RING_HW_TIMESTAMP; // hardware timestamp
  	//flags |= PF_RING_DNA_SYMMETRIC_RSS; // symmetric RSS, used by DNA drivers, ignored by all others
	pd = pfring_open( device, snaplen, flags);
	if ( pd == NULL) { printf( "ERROR! pfring_open() failed\n"); exit( 1); }
	
	
	startime = getime();
	for ( i = 0; i < num_threads; i++) lastimes[ i] = getime();
	u_int32_t version;
    	pfring_set_application_name( pd, "pfcount");
   	pfring_version( pd, &version);
   	printf( "pfring_open() OK, using PF_RING v.%d.%d.%d\n", ( version & 0xFFFF0000) >> 16, ( version & 0x0000FF00) >> 8, version & 0x000000FF);
   	
   	
   	printf( "Number of RX channels: %d\n", pfring_get_num_rx_channels( pd));
   	printf( "Polling threads: %d\n", num_threads);
   	signal( SIGINT, sigproc);
   	signal( SIGTERM, sigproc);
   	signal( SIGINT, sigproc);
   	
   	if ( num_threads <= 1) pfring_loop( pd, packet, (u_char*)NULL, wait_for_packet);
   	else { // multiple thread loop
		pthread_t my_thread;
		long i;
		for( i = 0; i < num_threads; i++) {
			pthread_create(&my_thread, NULL, packet_thread, (void*)i);
			usleep( 100000);
		}
		for( i = 0; i < num_threads; i++) pthread_join( my_thread, NULL);
	}
	
}

