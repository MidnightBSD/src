/* $MidnightBSD: src/lib/libc/ohash/ohash_int.h,v 1.1 2007/03/24 07:53:53 archite Exp $ */
/*	$OpenBSD: ohash_int.h,v 1.3 2006/01/16 15:52:25 espie Exp $	*/

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ohash.h"

struct _ohash_record {
	uint32_t	hv;
	const char 	*p;
};

#define DELETED		((const char *)h)
#define NONE		(h->size)

/* Don't bother changing the hash table if the change is small enough.  */
#define MINSIZE		(1UL << 4)
#define MINDELETED	4
