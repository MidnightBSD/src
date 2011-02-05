# $MidnightBSD: src/secure/usr.bin/scp/Makefile,v 1.5 2009/11/26 16:40:51 laffer1 Exp $

PROG=	scp
SRCS=	scp.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

# required when linkind dynamic libssh
SRCS+=	roaming_common.c roaming_serv.c

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
