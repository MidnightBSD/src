# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/i2c/controllers/Makefile 330680 2018-03-09 02:55:27Z rpokala $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm ismt nfsmb viapm lpbb pcf
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SUBDIR += imcsmb
.endif

.include <bsd.subdir.mk>
