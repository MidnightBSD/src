#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/9/libexec/tftpd/Makefile 247637 2013-03-02 16:33:43Z marius $

PROG=	tftpd
MAN=	tftpd.8
SRCS=	tftp-file.c tftp-io.c tftp-options.c tftp-transfer.c tftp-utils.c
SRCS+=	tftpd.c
WFORMAT=0

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
