#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/lib/libc/locale/nomacros.c 92986 2002-03-22 21:53:29Z obrien $");

/*
 * Tell <ctype.h> to generate extern versions of all its inline
 * functions.  The extern versions get called if the system doesn't
 * support inlines or the user defines _DONT_USE_CTYPE_INLINE_
 * before including <ctype.h>.
 */
#define _EXTERNALIZE_CTYPE_INLINES_

#include <ctype.h>
