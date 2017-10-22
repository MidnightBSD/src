# $FreeBSD: release/10.0.0/tools/tools/ncpus/Makefile 156362 2006-03-06 21:51:27Z sam $

PROG=	ncpus
NO_MAN=
SRCS=	ncpus.c acpi.c
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
SRCS+=	biosmptable.c
.endif
BINDIR=	/usr/local/bin
WARNS?=	6

DPADD=	${LIBDEVINFO}
LDADD=	-ldevinfo

.include <bsd.prog.mk>
