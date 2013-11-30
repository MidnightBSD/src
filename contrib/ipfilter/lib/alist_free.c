/*
 * Copyright (C) 2006 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: alist_free.c,v 1.1.1.1 2008-11-22 14:33:09 laffer1 Exp $
 */
#include "ipf.h"

void
alist_free(hosts)
alist_t *hosts;
{
	alist_t *a, *next;

	for (a = hosts; a != NULL; a = next) {
		next = a->al_next;
		free(a);
	}
}
