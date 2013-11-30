# $MidnightBSD$
# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.16 2004/12/21 10:16:03 ru Exp $

.include <bsd.own.mk>

SUBDIR=	lib add create delete info ${_sign} version

.if ${MK_OPENSSL} != "no"
_sign=	sign
.endif

.include <bsd.subdir.mk>
