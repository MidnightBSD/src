/*	$FreeBSD: release/10.0.0/contrib/ipfilter/lib/v6optvalue.c 255332 2013-09-06 23:11:19Z cy $	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */
#include "ipf.h"



u_32_t getv6optbyname(optname)
	char *optname;
{
#ifdef	USE_INET6
	struct ipopt_names *io;

	for (io = v6ionames; io->on_name; io++)
		if (!strcasecmp(optname, io->on_name))
			return io->on_bit;
#endif
	return -1;
}


u_32_t getv6optbyvalue(optval)
	int optval;
{
#ifdef	USE_INET6
	struct ipopt_names *io;

	for (io = v6ionames; io->on_name; io++)
		if (io->on_value == optval)
			return io->on_bit;
#endif
	return -1;
}
