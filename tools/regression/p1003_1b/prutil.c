#include <err.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "prutil.h"

/*
 * $FreeBSD: stable/11/tools/regression/p1003_1b/prutil.c 283018 2015-05-16 22:53:26Z ngie $
 */
void quit(const char *text)
{
	err(errno, "%s", text);
}

char *sched_text(int scheduler)
{
	switch(scheduler)
	{
		case SCHED_FIFO:
		return "SCHED_FIFO";

		case SCHED_RR:
		return "SCHED_RR";

		case SCHED_OTHER:
		return "SCHED_OTHER";

		default:
		return "Illegal scheduler value";
	}
}

int sched_is(int line, struct sched_param *p, int shouldbe)
{
	int scheduler;
	struct sched_param param;

	/* What scheduler are we running now?
	 */
	errno = 0;
	scheduler = sched_getscheduler(0);
	if (sched_getparam(0, &param))
		quit("sched_getparam");

	if (p)
		*p = param;

	if (shouldbe != -1 && scheduler != shouldbe)
	{
		fprintf(stderr,
		"At line %d the scheduler should be %s yet it is %s.\n",
		line, sched_text(shouldbe), sched_text(scheduler));

		exit(-1);
	}

	return scheduler;
}
