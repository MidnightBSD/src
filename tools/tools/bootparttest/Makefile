# $FreeBSD: release/10.0.0/tools/tools/bootparttest/Makefile 239070 2012-08-05 15:40:16Z ae $

.PATH:		${.CURDIR}/../../../sys/boot/common

BINDIR?=	/usr/bin

PROG=		bootparttest
NO_MAN=

SRCS=		bootparttest.c crc32.c malloc.c part.c

CFLAGS=		-I${.CURDIR}/../../../sys/boot/common -I. \
		-DLOADER_GPT_SUPPORT -DLOADER_MBR_SUPPORT -DPART_DEBUG

DPADD+=		${LIBGEOM} ${LIBUTIL}
LDADD+=		${LIBGEOM} ${LIBUTIL}
LDFLAGS+=	-lgeom -lutil

.include <bsd.prog.mk>
