/* lookup.h */
/* $FreeBSD: release/10.0.0/libexec/bootpd/lookup.h 97416 2002-05-28 18:31:41Z alfred $ */

#include "bptypes.h"	/* for int32, u_int32 */

extern u_char *lookup_hwa(char *hostname, int htype);
extern int lookup_ipa(char *hostname, u_int32 *addr);
extern int lookup_netmask(u_int32 addr, u_int32 *mask);
