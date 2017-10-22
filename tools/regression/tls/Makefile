# $FreeBSD: release/10.0.0/tools/regression/tls/Makefile 145467 2005-04-23 23:47:58Z davidxu $

SUBDIR=libxx libyy ttls1 ttls2 ttls4

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ttls3
.endif

.include <bsd.subdir.mk>
