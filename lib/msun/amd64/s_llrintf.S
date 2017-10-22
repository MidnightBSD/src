#include <machine/asm.h>
__FBSDID("$FreeBSD: release/7.0.0/lib/msun/amd64/s_llrintf.S 145171 2005-04-16 21:12:55Z das $")
	
/* sizeof(long) == sizeof(long long) */
#define	fn	llrintf
#include "s_lrintf.S"
