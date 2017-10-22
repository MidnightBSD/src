# $FreeBSD: release/10.0.0/etc/periodic/monthly/Makefile 254978 2013-08-27 22:37:29Z jkim $

.include <bsd.own.mk>

FILES=	450.status-security \
	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_UTMPX} != "no"
FILES+=	200.accounting
.endif

.include <bsd.prog.mk>
