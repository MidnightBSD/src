/*	$FreeBSD: stable/9/contrib/ipfilter/ipsend/linux.h 145519 2005-04-25 18:20:15Z darrenr $	*/

/*
 * Copyright (C) 1995-1998 by Darren Reed.
 *
 * This code may be freely distributed as long as it retains this notice
 * and is not changed in any way.  The author accepts no responsibility
 * for the use of this software.  I hate legaleese, don't you ?
 *
 * @(#)linux.h	1.1 8/19/95
 */

#include <linux/config.h>
#ifdef MODULE
#include <linux/module.h>
#include <linux/version.h>
#endif /* MODULE */

#include "ip_compat.h"
