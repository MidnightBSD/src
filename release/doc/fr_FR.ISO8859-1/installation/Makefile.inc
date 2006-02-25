#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id: Makefile.inc,v 1.1.1.2 2006-02-25 02:35:16 laffer1 Exp $
#      $FreeBSD: src/release/doc/fr_FR.ISO8859-1/installation/Makefile.inc,v 1.2 2002/05/26 16:49:48 gioria Exp $
#      Original revision: 1.2


.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/installation/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/fr_FR.ISO8859-1/installation/${.CURDIR:T}
.endif
