#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/libexec/tftpd/Makefile 235643 2012-05-19 05:10:47Z marcel $

PROG=	tftpd
MAN=	tftpd.8
SRCS=	tftp-file.c tftp-io.c tftp-options.c tftp-transfer.c tftp-utils.c
SRCS+=	tftpd.c
WFORMAT=0

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
