/*	$OpenBSD: ohash_int.h,v 1.3 2006/01/16 15:52:25 espie Exp $	*/
/* $FreeBSD: release/10.0.0/usr.bin/m4/lib/ohash_int.h 228063 2011-11-28 13:32:39Z bapt $ */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ohash.h"

struct _ohash_record {
	u_int32_t	hv;
	const char 	*p;
};

#define DELETED		((const char *)h)
#define NONE		(h->size)

/* Don't bother changing the hash table if the change is small enough.  */
#define MINSIZE		(1UL << 4)
#define MINDELETED	4
