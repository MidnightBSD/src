# $MidnightBSD: src/sbin/geom/class/Makefile,v 1.2 2011/12/28 00:41:18 laffer1 Exp $
# $FreeBSD: src/sbin/geom/class/Makefile,v 1.16 2007/09/23 07:34:22 pjd Exp $

.include <bsd.own.mk>

SUBDIR=	cache
SUBDIR+=concat
.if ${MK_OPENSSL} != "no"
SUBDIR+=eli
.endif
SUBDIR+=journal
SUBDIR+=label
SUBDIR+=mirror
SUBDIR+=mountver
SUBDIR+=multipath
SUBDIR+=nop
SUBDIR+=part
SUBDIR+=raid
SUBDIR+=raid3
SUBDIR+=raid5
SUBDIR+=sched
SUBDIR+=shsec
SUBDIR+=stripe
SUBDIR+=virstor

.include <bsd.subdir.mk>
