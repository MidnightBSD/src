#	@(#)Makefile	8.3 (Berkeley) 5/8/95

PACKAGE=runtime
MOUNT= ${.CURDIR}/../../sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c

CFLAGS+= -I${MOUNT}
CFLAGS+= -DMOUNT_CHAR_DEVS
SRCS+=	getmntopts.c

LIBADD=	xo util

.include <bsd.prog.mk>
