# $FreeBSD: stable/11/secure/usr.bin/ssh-add/Makefile 298107 2016-04-16 07:45:30Z gjb $

.include <src.opts.mk>

PROG=	ssh-add
SRCS+=	ssh-add.c
PACKAGE=	ssh
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
SRCS+=	ssh_namespace.h

LIBADD=	ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
#DPADD+=	${LIBLDNS}
#LDADD+=	-lldns
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
