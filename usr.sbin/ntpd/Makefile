# OpenNTPD 7.9p1 (portable) in-tree build glue.
#
# This reproduces by hand the autotools feature detection and per-OS compat
# object selection of the modern portable layout (src/, compat/, include/).
# See contrib/openntpd/MIDNIGHTBSD-upgrade for the capability mapping.

NTPD_DIR=	${SRCTOP}/contrib/openntpd
SYSCONFDIR=	/etc
LOCALSTATEDIR=	/var
RUNSTATEDIR=	/var/run

PROG=	ntpd
CONFS=	ntpd.conf

WARNS?=	2
# The upstream include/ compat shims re-typedef BSD types the base already
# provides (u_int8_t..u_int64_t in <sys/types.h>); silence just that so the
# rest of the tree's -Werror still applies.
CFLAGS+=	-Wno-typedef-redefinition

.PATH:	${NTPD_DIR}/src ${NTPD_DIR}/compat ${NTPD_DIR}

# Core ntpd sources.  HAVE_SENSORS is false (no sys/sensors.h) so fake-sensors.c
# is used; HAVE_LIBTLS is false (no libtls in base) so the HTTPS constraint is
# compiled out via constraint-disabled.c.
SRCS=	client.c config.c constraint-disabled.c control.c log.c ntp.c \
	ntp_dns.c ntp_msg.c ntpd.c parse.y fake-sensors.c server.c util.c

# Compat objects for functionality the MidnightBSD base does not provide.
# (imsg: not in libutil; adjfreq: not a syscall, adjfreq_freebsd.c wraps
# ntp_adjtime(2); recallocarray/freezero: not in libc.  socket.c, md5.c and
# progname.c are always built by the upstream compat library.)
SRCS+=	socket.c md5.c progname.c adjfreq_freebsd.c imsg.c imsg-buffer.c \
	recallocarray.c freezero.c

CFLAGS+=	-I${NTPD_DIR}/src -I${NTPD_DIR}/include -I${NTPD_DIR}/compat
CFLAGS+=	-DYYSTYPE_IS_DECLARED -DSHA2_SMALL
CFLAGS+=	-DSYSCONFDIR=\"${SYSCONFDIR}\"
CFLAGS+=	-DLOCALSTATEDIR=\"${LOCALSTATEDIR}\"
CFLAGS+=	-DRUNSTATEDIR=\"${RUNSTATEDIR}\"

# Features present in the MidnightBSD base system (verified against lib/libc,
# lib/libutil and sys/): take the native path, skip the compat implementation.
CFLAGS+=	-DHAVE_ASPRINTF -DHAVE_CLOSEFROM -DHAVE_DAEMON \
		-DHAVE_CLOCK_GETRES -DHAVE_CLOCK_GETTIME \
		-DHAVE_EXPLICIT_BZERO -DHAVE_MEMMEM -DHAVE_REALLOCARRAY \
		-DHAVE_SETPROCTITLE -DHAVE_SETGROUPS \
		-DHAVE_SETREUID -DHAVE_SETREGID \
		-DHAVE_SETRESUID -DHAVE_SETRESGID \
		-DHAVE_STRLCAT -DHAVE_STRLCPY -DHAVE_STRTONUM -DHAVE_SYSCONF \
		-DHAVE_ARC4RANDOM_ -DHAVE_ARC4RANDOM_UNIFORM \
		-DHAVE_ERR_H -DHAVE_PATHS_H -DHAVE_IFADDRS_H -DHAVE___PROGNAME \
		-DHAVE_MACHINE_ENDIAN_H -DHAVE_STRUCT_SOCKADDR_IN_SIN_LEN

LIBADD=	util m

# ntpctl is the same executable as ntpd, selected by argv[0].
LINKS=	${BINDIR}/ntpd ${BINDIR}/ntpctl

# Man pages are generated from the upstream *.in templates with the install
# paths substituted in.
MAN=	ntpd.8 ntpd.conf.5 ntpctl.8
CLEANFILES+=	ntpd.8 ntpd.conf.5 ntpctl.8

.for _m in ntpd.8 ntpd.conf.5 ntpctl.8
${_m}: ${NTPD_DIR}/src/${_m}.in
	sed -e 's,@sysconfdir@,${SYSCONFDIR},g' \
	    -e 's,@localstatedir@,${LOCALSTATEDIR},g' \
	    -e 's,@runstatedir@,${RUNSTATEDIR},g' \
	    -e 's,@sbindir@,${BINDIR},g' \
	    ${NTPD_DIR}/src/${_m}.in > ${.TARGET}
.endfor

.include <bsd.prog.mk>
