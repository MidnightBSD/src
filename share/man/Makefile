#	@(#)Makefile	8.2 (Berkeley) 4/16/94
# $FreeBSD: release/7.0.0/share/man/Makefile 174854 2007-12-22 06:32:46Z cvs2svn $

.include <bsd.own.mk>

# XXX MISSING:	man3f
SUBDIR=	man1 man3 man4 man5 man6 man7 man8 man9

MAKEWHATIS?=	makewhatis

makedb:
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/man
.if ${MK_OPENSSL} != "no"
	${MAKEWHATIS} ${DESTDIR}${BINDIR}/openssl/man
.endif

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.subdir.mk>
