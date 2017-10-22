# $FreeBSD: stable/9/secure/usr.bin/scp/Makefile 197679 2009-10-01 17:12:52Z des $

PROG=	scp
SRCS=	scp.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

# required when linking with a dynamic libssh
SRCS+=	roaming_dummy.c

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
