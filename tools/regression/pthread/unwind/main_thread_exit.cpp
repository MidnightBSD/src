/* $FreeBSD: release/10.0.0/tools/regression/pthread/unwind/main_thread_exit.cpp 213155 2010-09-25 04:26:40Z davidxu $ */
/* check unwinding for main thread */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "Test.cpp"

int
main()
{
	Test test;

	atexit(check_destruct);
	pthread_exit((void *)1);
	return (0);
}
