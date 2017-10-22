# $FreeBSD: release/10.0.0/sys/modules/i2c/controllers/Makefile 153618 2005-12-21 15:49:51Z ru $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
