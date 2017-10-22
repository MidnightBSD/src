#include <machine/asm.h>
__FBSDID("$FreeBSD: release/10.0.0/lib/msun/amd64/s_llrint.S 218305 2011-02-04 21:54:06Z kib $")
	
/* sizeof(long) == sizeof(long long) */
#define	fn	llrint
#include "s_lrint.S"
