/*	$FreeBSD: src/contrib/ipfilter/kmem.h,v 1.3 2005/12/30 11:52:22 guido Exp $	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 * $Id: kmem.h,v 1.1.1.2 2008-11-22 14:33:09 laffer1 Exp $
 */

#ifndef	__KMEM_H__
#define	__KMEM_H__

#ifndef	__P
# ifdef	__STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif
extern	int	openkmem __P((char *, char *));
extern	int	kmemcpy __P((char *, long, int));
extern	int	kstrncpy __P((char *, long, int));

#if defined(__NetBSD__) || defined(__OpenBSD)
# include <paths.h>
#endif

#ifdef _PATH_KMEM
# define	KMEM	_PATH_KMEM
#else
# define	KMEM	"/dev/kmem"
#endif

#endif /* __KMEM_H__ */
