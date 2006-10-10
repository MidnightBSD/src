# $MidnightBSD$
# $FreeBSD: src/secure/usr.bin/scp/Makefile,v 1.14 2005/06/07 09:31:26 des Exp $

PROG=	scp
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
