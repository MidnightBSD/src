#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/libc/gen/setprogname.c 93399 2002-03-29 22:43:43Z markm $");

#include <stdlib.h>
#include <string.h>

#include "libc_private.h"

void
setprogname(const char *progname)
{
	const char *p;

	p = strrchr(progname, '/');
	if (p != NULL)
		__progname = p + 1;
	else
		__progname = progname;
}
