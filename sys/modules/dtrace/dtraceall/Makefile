# $MidnightBSD: src/sys/modules/dtrace/dtraceall/Makefile,v 1.1 2012/03/28 21:16:38 laffer1 Exp $
# $FreeBSD: src/sys/modules/dtrace/dtraceall/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

KMOD=		dtraceall
SRCS=		dtraceall.c opt_compat.h

CFLAGS+=	-I${.CURDIR}/../../..

.if !defined(KERNBUILDDIR)
.if ${MACHINE_ARCH} == "amd64"
opt_compat.h:
	echo "#define COMPAT_FREEBSD32 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
