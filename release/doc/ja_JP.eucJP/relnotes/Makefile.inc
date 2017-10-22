# $FreeBSD: release/7.0.0/release/doc/ja_JP.eucJP/relnotes/Makefile.inc 82998 2001-09-04 17:48:07Z hrs $
# Original revision: 1.2

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes/${.CURDIR:T}
.else
DESTDIR?=	${DOCDIR}/ja_JP.eucJP/relnotes/${.CURDIR:T}
.endif

