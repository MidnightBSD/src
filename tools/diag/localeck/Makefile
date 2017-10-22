# $FreeBSD: release/10.0.0/tools/diag/localeck/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG=	localeck
NO_MAN=
WARNS?=	4

LOCALEDIR?= /usr/share/locale

test: ${PROG}
	sh docheck.sh ${LOCALEDIR}

.include <bsd.prog.mk>
