# $FreeBSD: release/7.0.0/sbin/ggate/Makefile 172500 2007-10-09 23:31:11Z obrien $

.include <bsd.own.mk>

SUBDIR=	${_ggatec} \
	${_ggated} \
	ggatel

.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_ggatec=	ggatec
_ggated=	ggated
.endif

.include <bsd.subdir.mk>
