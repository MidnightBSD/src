# $MidnightBSD$
# $FreeBSD: src/secure/usr.bin/ssh-agent/Makefile,v 1.14 2005/06/07 09:31:27 des Exp $

PROG=	ssh-agent
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
