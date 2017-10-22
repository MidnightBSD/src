# $FreeBSD: release/7.0.0/usr.sbin/pkg_install/Makefile 174854 2007-12-22 06:32:46Z cvs2svn $

.include <bsd.own.mk>

SUBDIR=	lib add create delete info ${_sign} version

.if ${MK_OPENSSL} != "no"
_sign=	sign
.endif

.include <bsd.subdir.mk>
