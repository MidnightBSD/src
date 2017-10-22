# $FreeBSD: release/10.0.0/usr.bin/iscsictl/Makefile 255570 2013-09-14 15:29:06Z trasz $

PROG=		iscsictl
SRCS=		iscsictl.c periphs.c parse.y token.l y.tab.h
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../sys/dev/iscsi
MAN=		iscsictl.8

DPADD=		${LIBCAM} ${LIBUTIL} 
LDADD=		-lcam -lfl -lutil

YFLAGS+=	-v
LFLAGS+=	-i
CLEANFILES=	y.tab.c y.tab.h y.output

WARNS=		6
NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.prog.mk>
