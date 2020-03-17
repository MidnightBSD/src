# $FreeBSD: stable/11/sys/modules/dtrace/dtraceall/Makefile 332566 2018-04-16 14:39:04Z lidl $

SYSDIR?=	${SRCTOP}/sys

KMOD=		dtraceall
SRCS=		dtraceall.c opt_compat.h opt_nfs.h

CFLAGS+=	-I${SYSDIR}

.if !defined(KERNBUILDDIR)
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "mips64"
opt_compat.h:
	echo "#define COMPAT_FREEBSD32 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
