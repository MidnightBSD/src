/*	$FreeBSD: release/10.0.0/contrib/ipfilter/lib/tcpflags.c 255332 2013-09-06 23:11:19Z cy $	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"


/*
 * ECN is a new addition to TCP - RFC 2481
 */
#ifndef TH_ECN
# define	TH_ECN  0x40
#endif
#ifndef TH_CWR
# define	TH_CWR  0x80
#endif

extern	char	flagset[];
extern	u_char	flags[];


u_char tcpflags(flgs)
	char *flgs;
{
	u_char tcpf = 0;
	char *s, *t;

	for (s = flgs; *s; s++) {
		if (*s == 'W')
			tcpf |= TH_CWR;
		else {
			if (!(t = strchr(flagset, *s))) {
				return 0;
			}
			tcpf |= flags[t - flagset];
		}
	}
	return tcpf;
}
