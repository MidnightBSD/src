# $FreeBSD: stable/11/sys/modules/i2c/controllers/Makefile 330679 2018-03-09 02:55:22Z rpokala $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichiic ichsmb intpm ismt nfsmb viapm lpbb pcf
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SUBDIR += imcsmb
.endif

.include <bsd.subdir.mk>
