# $FreeBSD: stable/11/usr.bin/smbutil/Makefile 319186 2017-05-30 04:27:05Z ngie $

PROG=	smbutil
SRCS=	smbutil.c dumptree.c login.c lookup.c view.c print.c

LIBADD=	smb

CONTRIBDIR=	${SRCTOP}/contrib/smbfs
CFLAGS+=	-I${CONTRIBDIR}/include

WARNS?=	0

.PATH:	${CONTRIBDIR}/smbutil

.include <bsd.prog.mk>
