/****************************************************************************
 *
 * Simple diff mode test.
 *
 * $FreeBSD: release/10.0.0/lib/libkse/test/hello_d.c 172491 2007-10-09 13:42:34Z obrien $
 *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <pthread.h>

void *
entry(void * a_arg)
{
	fprintf(stderr, "Hello world\n");

	return NULL;
}

int
main()
{
	pthread_t thread;
	int error;

	error = pthread_create(&thread, NULL, entry, NULL);
	if (error)
		fprintf(stderr, "Error in pthread_create(): %s\n",
			strerror(error));

	error = pthread_join(thread, NULL);
	if (error)
		fprintf(stderr, "Error in pthread_join(): %s\n",
			strerror(error));

	return 0;
}
