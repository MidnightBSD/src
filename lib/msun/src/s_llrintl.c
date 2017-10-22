#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/msun/src/s_llrintl.c 175309 2008-01-14 02:12:07Z das $");

#define type		long double
#define	roundit		rintl
#define dtype		long long
#define	fn		llrintl

#include "s_lrint.c"
