#include "gthr.h"


// #include "ext/gthr_curl.h"
//#include "gthr_net.h"
//#include "gthr_ssl_net.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

// experiments.
#include <stdio.h>

_Atomic int d = 0;

void
hi(void *arg)
{
	int *a = arg;
	printf("Arrived in gthr %d\n", *a);
	
	while(d < 10000000){
		d++;

		for(int i = 0; i < 1000; i++);
		gthr_yield();
	}
	printf("gthr %d finished counting\n", *a);
}

int main(int argc, char **argv) {
	int procs = 1;
	if(argc > 1)
		procs = atoi(argv[1]);

	printf("Test with procs = %d\n", procs);
	
	struct gthr_context gctx;
	gthr_context_init(&gctx);

	int i = 1;
	gthr_create_on(&gctx, hi, &i);
	int o = 2;
	gthr_create_on(&gctx, hi, &o);
	int y = 3;
	gthr_create_on(&gctx, hi, &y);
	int z = 4;
	gthr_create_on(&gctx, hi, &z);

	if(procs > 1)
		gthr_context_runners(&gctx, procs - 1);

	while(!gthr_context_run_once(&gctx));

	gthr_context_end_runners(&gctx);
	gthr_context_finish(&gctx);
	return 0;
}
