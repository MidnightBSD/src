#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/msun/src/s_llrintf.c 140088 2005-01-11 23:12:55Z das $");

#define type		float
#define	roundit		rintf
#define dtype		long long
#define	fn		llrintf

#include "s_lrint.c"
