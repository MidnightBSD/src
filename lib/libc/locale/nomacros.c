#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/libc/locale/nomacros.c 232626 2012-03-06 21:56:30Z dim $");

/*
 * Tell <ctype.h> to generate extern versions of all its inline
 * functions.  The extern versions get called if the system doesn't
 * support inlines or the user defines _DONT_USE_CTYPE_INLINE_
 * before including <ctype.h>.
 */
#define _EXTERNALIZE_CTYPE_INLINES_

/*
 * Also make sure <runetype.h> does not generate an inline definition
 * of __getCurrentRuneLocale().
 */
#define __RUNETYPE_INTERNAL

#include <ctype.h>
