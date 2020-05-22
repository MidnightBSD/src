# 	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: stable/11/usr.bin/login/Makefile 275042 2014-11-25 14:29:10Z bapt $

.include <src.opts.mk>

PROG=	login
SRCS=	login.c login_fbtab.c
CFLAGS+=-DLOGALL
LIBADD=	util pam

WARNS?=	5

.if ${MK_AUDIT} != "no"
SRCS+=	login_audit.c
CFLAGS+= -DUSE_BSM_AUDIT
LIBADD+=	bsm
.endif

.if ${MK_SETUID_LOGIN} != "no"
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
