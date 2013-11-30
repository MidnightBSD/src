/*	$FreeBSD: src/contrib/ipfilter/lib/tcp_flags.c,v 1.4 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2000-2004 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: tcp_flags.c,v 1.1.1.2 2008-11-22 14:33:10 laffer1 Exp $
 */

#include "ipf.h"

extern	char	flagset[];
extern	u_char	flags[];


u_char tcp_flags(flgs, mask, linenum)
char *flgs;
u_char *mask;
int    linenum;
{
	u_char tcpf = 0, tcpfm = 0;
	char *s;

	s = strchr(flgs, '/');
	if (s)
		*s++ = '\0';

	if (*flgs == '0') {
		tcpf = strtol(flgs, NULL, 0);
	} else {
		tcpf = tcpflags(flgs);
	}

	if (s != NULL) {
		if (*s == '0')
			tcpfm = strtol(s, NULL, 0);
		else
			tcpfm = tcpflags(s);
	}

	if (!tcpfm) {
		if (tcpf == TH_SYN)
			tcpfm = 0xff & ~(TH_ECN|TH_CWR);
		else
			tcpfm = 0xff & ~(TH_ECN);
	}
	*mask = tcpfm;
	return tcpf;
}
