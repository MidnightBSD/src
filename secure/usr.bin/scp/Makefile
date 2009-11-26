# $MidnightBSD: src/secure/usr.bin/scp/Makefile,v 1.4 2009/05/02 18:01:22 laffer1 Exp $

PROG=	scp
SRCS=	scp.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

# required when linkind dynamic libssh
SRCS+=	roaming_common.c

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
