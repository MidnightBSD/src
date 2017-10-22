# The FreeBSD Russian Documentation Project
#
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/installation/Makefile.inc,v 1.1 2004/06/24 07:21:47 den Exp $
# $FreeBSD: release/7.0.0/release/doc/ru_RU.KOI8-R/installation/Makefile.inc 131040 2004-06-24 08:39:20Z den $
#
# Original revision: 1.2
#

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/installation/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/ru_RU.KOI8-R/installation/${.CURDIR:T}
.endif
