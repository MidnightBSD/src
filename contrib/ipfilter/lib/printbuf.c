/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000-2004 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: printbuf.c,v 1.1.1.3 2012-07-21 15:01:08 laffer1 Exp $
 */

#include <ctype.h>

#include "ipf.h"


void printbuf(buf, len, zend)
char *buf;
int len, zend;
{
	char *s, c;
	int i;

	for (s = buf, i = len; i; i--) {
		c = *s++;
		if (ISPRINT(c))
			putchar(c);
		else
			printf("\\%03o", c);
		if ((c == '\0') && zend)
			break;
	}
}
