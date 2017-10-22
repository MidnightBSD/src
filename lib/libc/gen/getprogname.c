#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/libc/gen/getprogname.c 93399 2002-03-29 22:43:43Z markm $");

#include "namespace.h"
#include <stdlib.h>
#include "un-namespace.h"

#include "libc_private.h"

__weak_reference(_getprogname, getprogname);

const char *
_getprogname(void)
{

	return (__progname);
}
