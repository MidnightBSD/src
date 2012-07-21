/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: debug.c,v 1.1.1.3 2012-07-21 15:01:08 laffer1 Exp $
 */

#if defined(__STDC__)
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include <stdio.h>

#include "ipt.h"
#include "opts.h"


#ifdef	__STDC__
void	debug(char *fmt, ...)
#else
void	debug(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
	va_list pvar;

	va_start(pvar, fmt);

	if (opts & OPT_DEBUG)
		vprintf(fmt, pvar);
	va_end(pvar);
}
