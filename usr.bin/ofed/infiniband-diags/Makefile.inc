# $FreeBSD: stable/11/usr.bin/ofed/infiniband-diags/Makefile.inc 335253 2018-06-16 15:05:05Z kib $

_spath=	${SRCTOP}/contrib/ofed/infiniband-diags
.PATH: ${_spath}/src ${_spath}/man

BINDIR?= /usr/bin
SRCS+= ibdiag_common.c ibdiag_sa.c
CFLAGS+= -I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/infiniband
CFLAGS+= -DHAVE_CONFIG_H=1
CFLAGS+= -I${_spath} -I${_spath}/src
LIBADD+= osmcomp ibmad ibumad

