/*	$FreeBSD: stable/10/contrib/ipfilter/lib/printbuf.c 255332 2013-09-06 23:11:19Z cy $	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include <ctype.h>

#include "ipf.h"


void
printbuf(buf, len, zend)
	char *buf;
	int len, zend;
{
	char *s;
	int c;
	int i;

	for (s = buf, i = len; i; i--) {
		c = *s++;
		if (isprint(c))
			putchar(c);
		else
			PRINTF("\\%03o", c);
		if ((c == '\0') && zend)
			break;
	}
}
