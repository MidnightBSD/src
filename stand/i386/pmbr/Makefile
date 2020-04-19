# $FreeBSD: stable/11/stand/i386/pmbr/Makefile 329183 2018-02-12 20:51:28Z kevans $

PROG=	pmbr
STRIP=
BINMODE=${NOBINMODE}
SRCS=	${PROG}.s

ORG=	0x600

AFLAGS+=--defsym FLAGS=${BOOT_MBR_FLAGS}
LDFLAGS+=${LDFLAGS_BIN}

.include <bsd.prog.mk>
