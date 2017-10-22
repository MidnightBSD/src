# $FreeBSD: release/10.0.0/sys/modules/Makefile.inc 106497 2002-11-06 13:47:00Z nyan $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

