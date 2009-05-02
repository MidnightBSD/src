# $MidnightBSD: src/secure/usr.bin/ssh-keygen/Makefile,v 1.2 2006/10/10 18:52:38 laffer1 Exp $

PROG=	ssh-keygen
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
