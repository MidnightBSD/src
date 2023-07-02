
#include <stdio.h>
#include <stdlib.h>

#include <dispatch/dispatch.h>

void deferred(void *);

int 
main(int argc, char * argv[])
{
	dispatch_queue_t queue;
	dispatch_time_t dt;

	queue = dispatch_get_main_queue();
	dt = dispatch_time(DISPATCH_TIME_NOW, 3LL * NSEC_PER_SEC);

	dispatch_after_f(dt, queue, NULL, deferred);

	dispatch_main();
	return 0;
}

void 
deferred(void *arg)
{
	puts("dispatched block executed");
	exit(0);
}
