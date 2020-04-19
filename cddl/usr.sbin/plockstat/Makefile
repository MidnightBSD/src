# $FreeBSD: stable/11/cddl/usr.sbin/plockstat/Makefile 321268 2017-07-20 00:53:50Z ngie $

.PATH: ${SRCTOP}/cddl/contrib/opensolaris/cmd/plockstat

PROG=		plockstat
SRCS=		plockstat.c
BINDIR?=	/usr/sbin

WARNS?=		1

CFLAGS+=	-I${SRCTOP}/sys/cddl/compat/opensolaris \
		-I${SRCTOP}/cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libproc/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/compat \
		-I${SRCTOP}/cddl/lib/libdtrace \
		-I${SRCTOP}/sys

LIBADD=	dtrace proc

.include <bsd.prog.mk>
