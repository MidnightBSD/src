# $FreeBSD: release/10.0.0/sbin/ggate/Makefile 177714 2008-03-29 17:44:40Z ru $

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBTHR} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
