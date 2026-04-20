#
# MidnightBSD prowld(8) - Service Management Daemon
#

PACKAGE=	prowld
PROG=		prowld
SRCS=		prowld.c \
		job.c \
		unit.c \
		rcshim.c \
		dag.c \
		supervisor.c \
		socket_activation.c \
		ipc.c
MAN=		prowld.8 prowld.conf.5 prowl-unit.5

CFLAGS+=	-I${SRCTOP}/contrib/libucl/include
LIBADD=		ucl util

WARNS?=		6

CONFS=		prowld.conf.sample
CONFSDIR=	/etc/prowld

.include <bsd.prog.mk>
