# $FreeBSD: src/usr.bin/tar/Makefile,v 1.32.2.4 2008/08/25 02:18:12 kientzle Exp $

PROG=	bsdtar
BSDTAR_VERSION_STRING=2.5.5
SRCS=	bsdtar.c getdate.y matching.c read.c siginfo.c subst.c tree.c util.c write.c
WARNS?=	5
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD=	-larchive -lbz2 -lz
CFLAGS+=	-DBSDTAR_VERSION_STRING=\"${BSDTAR_VERSION_STRING}\"
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_midnightbsd.h\"
CFLAGS+=	-I${.CURDIR}
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1

.include <bsd.prog.mk>
