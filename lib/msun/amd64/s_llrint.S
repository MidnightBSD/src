#include <machine/asm.h>
__FBSDID("$FreeBSD: release/7.0.0/lib/msun/amd64/s_llrint.S 140275 2005-01-15 03:32:28Z das $")
	
/* sizeof(long) == sizeof(long long) */
#define	fn	llrint
#include "s_lrint.S"
