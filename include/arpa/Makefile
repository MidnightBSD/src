# $FreeBSD: stable/9/include/arpa/Makefile 248357 2013-03-15 22:26:19Z brooks $

.include <bsd.own.mk>

NO_OBJ=
INCS=	ftp.h inet.h nameser.h nameser_compat.h tftp.h
.if ${MK_TELNET} == "no"
INCS+=	telnet.h
.endif
INCSDIR=${INCLUDEDIR}/arpa

.include <bsd.prog.mk>
