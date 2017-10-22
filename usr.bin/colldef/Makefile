# $FreeBSD: release/10.0.0/usr.bin/colldef/Makefile 215676 2010-11-22 14:16:22Z brucec $

PROG=	colldef
SRCS=	parse.y scan.l y.tab.h
LFLAGS=	-8 -i
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale
CFLAGS+=-DCOLLATE_DEBUG -DYY_NO_UNPUT -DYY_NO_INPUT
LDADD=	-ll
DPADD=	${LIBL}

WARNS?=	2

.include <bsd.prog.mk>
