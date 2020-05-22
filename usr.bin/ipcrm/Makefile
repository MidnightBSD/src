# $FreeBSD: stable/11/usr.bin/ipcrm/Makefile 319186 2017-05-30 04:27:05Z ngie $

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
LIBADD=	kvm
CFLAGS+=-I${SRCTOP}/usr.bin/ipcs
.PATH:  ${SRCTOP}/usr.bin/ipcs

WARNS?=	0

.include <bsd.prog.mk>
