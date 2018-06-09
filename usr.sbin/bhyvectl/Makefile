# $MidnightBSD$
#
# $FreeBSD: stable/10/usr.sbin/bhyvectl/Makefile 294790 2016-01-26 09:50:36Z brueffer $
#

PROG=	bhyvectl
SRCS=	bhyvectl.c

MAN=	bhyvectl.8

DPADD=	${LIBVMMAPI} ${LIBUTIL}
LDADD=	-lvmmapi -lutil

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../../sys/amd64/vmm

.include <bsd.prog.mk>
