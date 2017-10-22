# $FreeBSD: stable/9/etc/periodic/monthly/Makefile 223209 2011-06-17 21:30:21Z ed $

.include <bsd.own.mk>

FILES=	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_UTMPX} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
