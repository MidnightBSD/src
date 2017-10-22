# $FreeBSD: stable/9/secure/libexec/ssh-pkcs11-helper/Makefile 204948 2010-03-10 11:05:31Z des $

PROG=	ssh-pkcs11-helper
SRCS=	ssh-pkcs11.c ssh-pkcs11-helper.c
SRCS+=	roaming_dummy.c
MAN=	ssh-pkcs11-helper.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
