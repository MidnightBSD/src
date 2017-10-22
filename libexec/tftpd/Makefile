#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/7.0.0/libexec/tftpd/Makefile 74814 2001-03-26 14:22:12Z ru $

PROG=	tftpd
SRCS=	tftpd.c tftpsubs.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	tftpd.8
CFLAGS+=-I${.CURDIR}/../../usr.bin/tftp
.PATH:	${.CURDIR}/../../usr.bin/tftp

.include <bsd.prog.mk>
