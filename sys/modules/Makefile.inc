# $FreeBSD: stable/11/sys/modules/Makefile.inc 290665 2015-11-10 17:15:36Z bdrewery $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

SUBDIR_PARALLEL=	yes
