# $MidnightBSD$
# $FreeBSD: src/usr.bin/grep/nls/Makefile.inc,v 1.5 2010/09/20 19:42:52 delphij Exp $
NLSNAME= grep

NLS=	es_ES.ISO8859-1
NLS+=	gl_ES.ISO8859-1
NLS+=	hu_HU.ISO8859-2
NLS+=	ja_JP.eucJP
NLS+=	ja_JP.SJIS
NLS+=	ja_JP.UTF-8
NLS+=	pt_BR.ISO8859-1
NLS+=	ru_RU.KOI8-R
NLS+=	uk_UA.UTF-8
NLS+=	zh_CN.UTF-8

NLSSRCDIR= ${.CURDIR}/nls
.for lang in ${NLS}
NLSSRCFILES_${lang}=${lang}.msg
.endfor
