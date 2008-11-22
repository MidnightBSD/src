/*	$FreeBSD: src/contrib/ipfilter/lib/flags.c,v 1.4 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2001-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: flags.c,v 1.1.1.2 2008-11-22 14:33:09 laffer1 Exp $
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

char	flagset[] = "FSRPAUEC";
u_char	flags[] = { TH_FIN, TH_SYN, TH_RST, TH_PUSH, TH_ACK, TH_URG,
		    TH_ECN, TH_CWR };
