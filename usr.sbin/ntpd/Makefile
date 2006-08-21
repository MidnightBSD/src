# $MidnightBSD$

NTPD_DIR=${.CURDIR}/../../contrib/openntpd
NTPD_CDIR=${NTPD_DIR}/openbsd-compat

PROG=	ntpd

.PATH: ${NTPD_CDIR}
SRCS+=	asprintf.c atomicio.c daemon.c errx.c inet_pton.c \
	strlcpy.c verrx.c bsd-arc4random.c bsd-misc.c \
	bsd-poll.c bsd-snprintf.c \
	bsd-getifaddrs.c bsd-setresuid.c \
	bsd-setresgid.c fake-rfc2553.c

.PATH: ${NTPD_DIR}
SRCS+=	ntpd.c buffer.c log.c imsg.c ntp.c ntp_msg.c config.c \
	server.c client.c util.c y.tab.c

MAN=	ntpd.8 ntpd.conf.5
CFLAGS+=-I${NTPD_DIR} -I${NTPD_CDIR}

.include <bsd.prog.mk>

