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
UNITS=		units/aged.unit \
		units/auditd.unit \
		units/auditdistd.unit \
		units/automountd.unit \
		units/autounmountd.unit \
		units/blocklistd.unit \
		units/bootparamd.unit \
		units/bsnmpd.unit \
		units/bthidd.unit \
		units/cron.unit \
		units/ctld.unit \
		units/devd.unit \
		units/dhclient@.unit \
		units/dhcp6c.unit \
		units/ftpd.unit \
		units/ftpproxy@.unit \
		units/gssd.unit \
		units/hastd.unit \
		units/hcsecd.unit \
		units/hostapd.unit \
		units/hostapd@.unit \
		units/inetd.unit \
		units/ipfilter.unit \
		units/ipfs.unit \
		units/ipfw.unit \
		units/ipfw_netflow.unit \
		units/ipmon.unit \
		units/ipnat.unit \
		units/ippool.unit \
		units/iscsictl.unit \
		units/iscsid.unit \
		units/keyserv.unit \
		units/local_unbound.unit \
		units/lockd.unit \
		units/lpd.unit \
		units/mdnsd.unit \
		units/mdnsresponder.unit \
		units/mountd.unit \
		units/msgs.unit \
		units/natd.unit \
		units/newsyslog.unit \
		units/nfscbd.unit \
		units/nfsd.unit \
		units/nfsuserd.unit \
		units/nscd.unit \
		units/ntpd.unit \
		units/pf.unit \
		units/pflog@.unit \
		units/pfsync.unit \
		units/powerd.unit \
		units/pppoed.unit \
		units/rarpd.unit \
		units/rdate.unit \
		units/route6d.unit \
		units/routed.unit \
		units/rpcbind.unit \
		units/rtadvd.unit \
		units/rtsold.unit \
		units/rwhod.unit \
		units/sdpd.unit \
		units/sppp.unit \
		units/sshd.unit \
		units/statd.unit \
		units/syslogd.unit \
		units/tlsclntd.unit \
		units/tlsservd.unit \
		units/ubthidhci.unit \
		units/watchdogd.unit \
		units/wpa_supplicant@.unit \
		units/ypbind.unit \
		units/ypserv.unit \
		units/ypxfrd.unit \
		units/zfsd.unit
UNITSDIR=	/etc/prowld/units.d
UNITSMODE=	444

.include <bsd.prog.mk>
