# $MidnightBSD: src/secure/usr.bin/ssh-keyscan/Makefile,v 1.3 2009/05/02 18:01:23 laffer1 Exp $

PROG=	ssh-keyscan
SRCS=	ssh-keyscan.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

# required when linking dynamic libssh
SRCS+=	roaming_dummy.c

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
