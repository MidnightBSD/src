# $MidnightBSD: src/sbin/dhcp6c/Makefile,v 1.1 2012/01/14 15:18:41 laffer1 Exp $

.PATH:	${SRCTOP}/contrib/wide-dhcpv6

PACKAGE=	runtime
PROG=	dhcp6c
CONF=	dhcp6c.conf

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../contrib/wide-dhcpv6 -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" \
	-DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" \
	-DYYTEXT_POINTER=1 -DINET6=1 -DHAVE_GETADDRINFO=1 -DHAVE_GETNAMEINFO=1 \
	-DHAVE_GETIFADDRS=1 -DHAVE_IF_NAMETOINDEX=1 -DHAVE_STRLCPY=1 -DHAVE_STRLCAT=1 \
	-DHAVE_DAEMON=1 -DHAVE_WARNX=1 -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 \
	-DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 \
	-DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 \
	-DHAVE_FCNTL_H=1 -DHAVE_SYS_IOCTL_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYSLOG_H=1 \
	-DHAVE_UNISTD_H=1 -DHAVE_IFADDRS_H=1 -DTIME_WITH_SYS_TIME=1 \
	-DHAVE_STRUCT_TM_TM_ZONE=1 -DHAVE_TM_ZONE=1 -DHAVE_SIG_ATOMIC_T=1 -DGETPGRP_VOID=1 \
	-DRETSIGTYPE=void -DHAVE_MKTIME=1 -DHAVE_SELECT=1 -DHAVE_SOCKET=1 \
	-DHAVE_CLOCK_GETTIME=1 -DHAVE_ARC4RANDOM=1 -DHAVE_ANSI_FUNC=1 \
	-DHAVE_TAILQ_FOREACH_REVERSE_OLD=1 -DHAVE_SA_LEN=1 -DHAVE_STDARG_H=1 \
	-DSYSCONFDIR=\"/etc\" -DLOCALDBDIR=\"/var/db\"

SRCS=	cftoken.l cfparse.y y.tab.h dhcp6c.c common.c config.c prefixconf.c dhcp6c_ia.c timer.c \
        dhcp6c_script.c if.c base64.c auth.c dhcp6_ctl.c addrconf.c lease.c

YFLAGS+= -d

CLEANFILES+=     y.tab.c y.tab.h y.output

MAN=	dhcp6c.8 dhcp6c.conf.5

WARNS?=	0

.include <bsd.prog.mk>

XCFLAGS.cftoken+= -Wno-missing-variable-declarations \
        -Wno-unneeded-internal-declaration
XCFLAGS.cfparse+= -Wno-missing-variable-declarations
CFLAGS+=	${XCFLAGS.${.TARGET:T:R}:U}
