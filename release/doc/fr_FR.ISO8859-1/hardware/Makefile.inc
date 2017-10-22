#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id$
#      $FreeBSD: stable/9/release/doc/fr_FR.ISO8859-1/hardware/Makefile.inc 109488 2003-01-18 18:03:01Z gioria $
#      Original revision: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/hardware/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/fr_FR.ISO8859-1/hardware/${.CURDIR:T}
.endif
