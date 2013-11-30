/*	$FreeBSD: src/contrib/ipfilter/lib/buildopts.c,v 1.2 2005/04/25 18:20:12 darrenr Exp $	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: buildopts.c,v 1.6 2002/01/28 06:50:45 darrenr Exp
 */

#include "ipf.h"


u_32_t buildopts(cp, op, len)
char *cp, *op;
int len;
{
	struct ipopt_names *io;
	u_32_t msk = 0;
	char *s, *t;
	int inc;

	for (s = strtok(cp, ","); s; s = strtok(NULL, ",")) {
		if ((t = strchr(s, '=')))
			*t++ = '\0';
		for (io = ionames; io->on_name; io++) {
			if (strcasecmp(s, io->on_name) || (msk & io->on_bit))
				continue;
			if ((inc = addipopt(op, io, len, t))) {
				op += inc;
				len += inc;
			}
			msk |= io->on_bit;
			break;
		}
		if (!io->on_name) {
			fprintf(stderr, "unknown IP option name %s\n", s);
			return 0;
		}
	}
	*op++ = IPOPT_EOL;
	len++;
	return len;
}
