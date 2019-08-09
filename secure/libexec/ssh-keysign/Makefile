# $MidnightBSD$
# $FreeBSD: stable/10/secure/libexec/ssh-keysign/Makefile 296781 2016-03-12 23:53:20Z des $

.include <bsd.own.mk>

PROG=	ssh-keysign
SRCS=	ssh-keysign.c readconf.c uidswap.c
MAN=	ssh-keysign.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
SRCS+=	ssh_namespace.h
BINMODE=4555

DPADD=	${LIBSSH}
LDADD=	-lssh
USEPRIVATELIB= ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
#DPADD+=	${LIBLDNS}
#LDADD+=	-lldns
#USEPRIVATELIB+= ldns
.endif

DPADD+= ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD+= -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
