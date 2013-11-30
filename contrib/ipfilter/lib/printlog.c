/*	$FreeBSD: src/contrib/ipfilter/lib/printlog.c,v 1.5 2007/06/04 02:54:32 darrenr Exp $	*/

/*
 * Copyright (C) 2000-2005 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printlog.c,v 1.1.1.2 2008-11-22 14:33:10 laffer1 Exp $
 */

#include "ipf.h"

#include <syslog.h>


void printlog(fp)
frentry_t *fp;
{
	char *s, *u;

	printf("log");
	if (fp->fr_flags & FR_LOGBODY)
		printf(" body");
	if (fp->fr_flags & FR_LOGFIRST)
		printf(" first");
	if (fp->fr_flags & FR_LOGORBLOCK)
		printf(" or-block");
	if (fp->fr_loglevel != 0xffff) {
		printf(" level ");
		s = fac_toname(fp->fr_loglevel);
		if (s == NULL || *s == '\0')
			s = "!!!";
		u = pri_toname(fp->fr_loglevel);
		if (u == NULL || *u == '\0')
			u = "!!!";
		printf("%s.%s", s, u);
	}
}
