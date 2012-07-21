/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: initparse.c,v 1.1.1.3 2012-07-21 15:01:08 laffer1 Exp $
 */
#include "ipf.h"


char	thishost[MAXHOSTNAMELEN];


void initparse __P((void))
{
	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';
}
