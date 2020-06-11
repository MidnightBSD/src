# $FreeBSD: stable/11/usr.sbin/etcupdate/Makefile 265420 2014-05-06 04:22:01Z imp $

.include <src.opts.mk>

SCRIPTS=etcupdate.sh
MAN=	etcupdate.8

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
