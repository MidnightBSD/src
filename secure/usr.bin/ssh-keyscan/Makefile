# $FreeBSD: stable/9/secure/usr.bin/ssh-keyscan/Makefile 197679 2009-10-01 17:12:52Z des $

PROG=	ssh-keyscan
SRCS=	ssh-keyscan.c roaming_dummy.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
