# $MidnightBSD$
#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: stable/10/bin/df/Makefile 310380 2016-12-21 23:59:58Z brooks $

MOUNT=	${.CURDIR}/../../sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

CFLAGS+= -DMOUNT_CHAR_DEVS
SRCS+=	getmntopts.c

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
