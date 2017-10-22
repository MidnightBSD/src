#ifndef _PRUTIL_H_
#define _PRUTIL_H_

/*
 * $FreeBSD: release/10.0.0/tools/regression/p1003_1b/prutil.h 57257 2000-02-16 14:28:42Z dufault $
 */

struct sched_param;

void quit(const char *);
char *sched_text(int);
int sched_is(int line, struct sched_param *, int);

#endif /* _PRUTIL_H_ */
