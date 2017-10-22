#
# $FreeBSD: release/10.0.0/usr.sbin/bhyvectl/Makefile 256176 2013-10-09 03:56:07Z neel $
#

PROG=	bhyvectl
SRCS=	bhyvectl.c

NO_MAN=

DPADD=	${LIBVMMAPI} ${LIBUTIL}
LDADD=	-lvmmapi -lutil

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../../sys/amd64/vmm

.include <bsd.prog.mk>
