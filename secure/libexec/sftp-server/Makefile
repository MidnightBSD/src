# $MidnightBSD$
# $FreeBSD: src/secure/libexec/sftp-server/Makefile,v 1.13 2005/06/07 09:31:26 des Exp $

PROG=   sftp-server
SRCS=   sftp-server.c sftp-common.c sftp-server-main.c
MAN=	sftp-server.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
