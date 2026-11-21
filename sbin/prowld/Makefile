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
UNITS=		units/blacklistd.unit \
		units/blocklistd.unit \
		units/bthidd.unit \
		units/cron.unit \
		units/devd.unit \
		units/ftpd.unit \
		units/gssd.unit \
		units/hastd.unit \
		units/hcsecd.unit \
		units/inetd.unit \
		units/iscsid.unit \
		units/local_unbound.unit \
		units/lockd.unit \
		units/lpd.unit \
		units/mdnsd.unit \
		units/mdnsresponder.unit \
		units/mountd.unit \
		units/moused.unit \
		units/natd.unit \
		units/newsyslog.unit \
		units/nfscbd.unit \
		units/nfsd.unit \
		units/nfsuserd.unit \
		units/nscd.unit \
		units/ntpd.unit \
		units/powerd.unit \
		units/pppoed.unit \
		units/rdate.unit \
		units/rpcbind.unit \
		units/rtadvd.unit \
		units/rtsold.unit \
		units/sdpd.unit \
		units/sshd.unit \
		units/statd.unit \
		units/syslogd.unit \
		units/watchdogd.unit \
		units/ypxfrd.unit \
		units/zfsd.unit

.include <bsd.prog.mk>
