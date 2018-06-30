# $MidnightBSD$
# $FreeBSD: stable/10/bin/ps/Makefile 266279 2014-05-17 02:32:47Z bdrewery $
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c

#
# To support "lazy" ps for non root/wheel users
# add -DLAZY_PS to the cflags.  This helps
# keep ps from being an unnecessary load
# on large systems.
#
CFLAGS+=-DLAZY_PS
DPADD=	${LIBM} ${LIBKVM} ${LIBJAIL}
LDADD=	-lm -lkvm -ljail

.include <bsd.prog.mk>
