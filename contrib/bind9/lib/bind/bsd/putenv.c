#ifndef LINT
static const char rcsid[] = "$Id: putenv.c,v 1.1.1.3 2008-04-18 18:31:33 laffer1 Exp $";
#endif

#include "port_before.h"
#include "port_after.h"

/*%
 * To give a little credit to Sun, SGI,
 * and many vendors in the SysV world.
 */

#if !defined(NEED_PUTENV)
int __bindcompat_putenv;
#else
int
putenv(char *str) {
	char *tmp;

	for (tmp = str; *tmp && (*tmp != '='); tmp++)
		;

	return (setenv(str, tmp, 1));
}
#endif

/*! \file */
