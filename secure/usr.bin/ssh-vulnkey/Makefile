# $MidnightBSD: src/secure/usr.bin/ssh-vulnkey/Makefile,v 1.1 2008/05/16 20:36:40 laffer1 Exp $

PROG=   ssh-vulnkey
SRCS=	ssh-vulnkey.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

# required when linking dynamic libssh
SRCS+=	roaming_dummy.c

DPADD=  ${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=  -lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:  ${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h

