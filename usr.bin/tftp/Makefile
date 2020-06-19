#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/tftp/Makefile 319186 2017-05-30 04:27:05Z ngie $

.PATH: ${SRCTOP}/libexec/tftpd

PROG=	tftp
SRCS=	main.c tftp-file.c tftp-io.c tftp-options.c tftp-transfer.c
SRCS+=	tftp-utils.c tftp.c
CFLAGS+=-I${SRCTOP}/libexec/tftpd

LIBADD=	edit

.include <bsd.prog.mk>
