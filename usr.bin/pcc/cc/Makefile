# $MidnightBSD$

PROG=	cc
PREFIX=	/usr/local
BINDIR=	${PREFIX}/bin
MANDIR=	${PREFIX}/man
TARGOS=	midnightbsd
TARGMACH=	${MACHINE_ARCH}

CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}\"
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Dmach_${TARGMACH} -Dos_${TARGOS}

.include <bsd.prog.mk>
