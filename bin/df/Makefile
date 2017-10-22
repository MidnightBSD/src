#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: release/10.0.0/bin/df/Makefile 129678 2004-05-24 22:22:29Z pjd $

MOUNT=	${.CURDIR}/../../sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
