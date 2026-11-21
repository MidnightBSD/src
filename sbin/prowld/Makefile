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
		ipc.c \
		pid1.c
MAN=		prowld.8 prowld.conf.5 prowl-unit.5

CFLAGS+=	-I${SRCTOP}/contrib/libucl/include
LIBADD=		ucl util crypt

WARNS?=		6

# prowld can be init(8): /usr is not mounted when it starts, and a reroot
# must be able to unmount the old root while it runs, so it cannot map
# shared libraries.  Like init(8), build it statically.
NO_SHARED?=	YES

CONFS=		prowld.conf.sample
CONFSDIR=	/etc/prowld

# Native units for base services.
FILESGROUPS+=	UNITS
UNITS=		units/syslogd.unit
UNITSDIR=	/etc/prowld/units.d
UNITSMODE=	444

.include <bsd.prog.mk>
