# $FreeBSD: release/10.0.0/usr.sbin/iscsid/Makefile 255570 2013-09-14 15:29:06Z trasz $

PROG=		iscsid
SRCS=		discovery.c iscsid.c keys.c log.c login.c pdu.c
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../sys/cam
CFLAGS+=	-I${.CURDIR}/../../sys/dev/iscsi
#CFLAGS+=	-DICL_KERNEL_PROXY
MAN=		iscsid.8

DPADD=		${LIBUTIL}
LDADD=		-lcrypto -lssl -lutil

WARNS=		6

.include <bsd.prog.mk>
