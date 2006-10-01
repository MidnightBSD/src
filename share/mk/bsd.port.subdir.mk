# $FreeBSD: src/share/mk/bsd.port.subdir.mk,v 1.31 2004/07/02 20:47:18 eik Exp $
# $MidnightBSD: src/share/mk/bsd.port.subdir.mk,v 1.2 2006/05/22 06:03:21 laffer1 Exp $

PORTSDIR?=	/usr/mports
BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
