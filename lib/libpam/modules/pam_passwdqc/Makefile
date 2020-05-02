# $FreeBSD: stable/11/lib/libpam/modules/pam_passwdqc/Makefile 313538 2017-02-10 07:32:40Z ngie $

SRCDIR=	${SRCTOP}/contrib/pam_modules/pam_passwdqc
.PATH: ${SRCDIR}

LIB=	pam_passwdqc
SRCS=	pam_passwdqc.c passwdqc_check.c passwdqc_random.c wordset_4k.c
MAN=	pam_passwdqc.8

WARNS?=	2
CFLAGS+= -I${SRCDIR}

LIBADD+=	crypt

.include <bsd.lib.mk>

