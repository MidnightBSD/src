#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: stable/11/usr.bin/su/Makefile 275042 2014-11-25 14:29:10Z bapt $

.include <src.opts.mk>

PROG=	su

WARNS?=	5

LIBADD=	util pam

.if ${MK_AUDIT} != "no"
CFLAGS+= -DUSE_BSM_AUDIT
LIBADD+=	bsm
.endif

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

.include <bsd.prog.mk>
