# $MidnightBSD: src/secure/libexec/sftp-server/Makefile,v 1.4 2009/05/02 18:00:56 laffer1 Exp $

PROG=   sftp-server
SRCS=   sftp-server.c sftp-common.c sftp-server-main.c roaming_dummy.c
MAN=	sftp-server.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
