#	@(#)Makefile	8.1 (Berkeley) 6/9/93
# $FreeBSD: stable/11/usr.bin/tset/Makefile 275042 2014-11-25 14:29:10Z bapt $

PROG=	tset
SRCS=	map.c misc.c set.c term.c tset.c wrterm.c

LIBADD=	ncursesw
LINKS=	${BINDIR}/tset ${BINDIR}/reset
MLINKS=	tset.1 reset.1

.include <bsd.prog.mk>
