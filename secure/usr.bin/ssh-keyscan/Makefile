# $MidnightBSD$
# $FreeBSD: src/secure/usr.bin/ssh-keyscan/Makefile,v 1.9 2005/06/07 09:31:28 des Exp $

PROG=	ssh-keyscan
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
