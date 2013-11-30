# $FreeBSD: src/share/mk/bsd.port.mk,v 1.307 2004/07/02 20:47:18 eik Exp $
# $MidnightBSD: src/share/mk/bsd.port.mk,v 1.6 2007/04/15 04:55:21 laffer1 Exp $

PORTSDIR?=	/usr/mports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.mport.mk

.include <bsd.own.mk>
.include "${BSDPORTMK}"
