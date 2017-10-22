#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/msun/src/s_llroundl.c 144772 2005-04-08 01:24:08Z das $");

#define type		long double
#define	roundit		roundl
#define dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llroundl

#include "s_lround.c"
