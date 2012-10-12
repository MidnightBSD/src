# $MidnightBSD: src/sys/modules/dtrace/Makefile,v 1.1 2012/03/28 21:16:38 laffer1 Exp $
# $FreeBSD: src/sys/modules/dtrace/Makefile,v 1.4.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>
.include "Makefile.inc"

SUBDIR=		dtmalloc	\
		dtnfscl		\
		dtnfsclient	\
		dtrace		\
		dtraceall	\
		dtrace_test	\
		lockstat	\
		profile		\
		prototype	\
		sdt		\
		systrace

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	fasttrap fbt systrace_linux32
.endif
.if ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	systrace_freebsd32
.endif

.include <bsd.subdir.mk>
