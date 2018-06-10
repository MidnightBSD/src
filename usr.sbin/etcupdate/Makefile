# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/etcupdate/Makefile 264996 2014-04-27 01:15:10Z jmmv $

.include <bsd.own.mk>

SCRIPTS=etcupdate.sh
MAN=	etcupdate.8

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
