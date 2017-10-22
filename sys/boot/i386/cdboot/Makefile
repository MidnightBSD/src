# $FreeBSD: release/7.0.0/sys/boot/i386/cdboot/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x7c00

LDFLAGS=-N -e start -Ttext ${ORG} -Wl,-S,--oformat,binary

.include <bsd.prog.mk>
