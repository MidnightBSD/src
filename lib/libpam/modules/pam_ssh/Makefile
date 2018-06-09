# $MidnightBSD$
# PAM module for SSH
# $FreeBSD: stable/10/lib/libpam/modules/pam_ssh/Makefile 313579 2017-02-11 05:41:53Z ngie $

SSHDIR=	${SRCTOP}/crypto/openssh

LIB=	pam_ssh
MAN=	pam_ssh.8
SRCS=	pam_ssh.c

WARNS?=	5
CFLAGS+= -I${SSHDIR} -include ssh_namespace.h
SRCS+=	 ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lssh -lcrypto -lcrypt
USEPRIVATELIB= ssh

.include <bsd.lib.mk>

.PATH: ${SSHDIR}
