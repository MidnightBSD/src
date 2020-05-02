#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/11/lib/msun/src/s_lrintl.c 175309 2008-01-14 02:12:07Z das $");

#define type		long double
#define	roundit		rintl
#define dtype		long
#define	fn		lrintl

#include "s_lrint.c"
