# $FreeBSD: src/share/mk/bsd.port.mk,v 1.307 2004/07/02 20:47:18 eik Exp $
# $MidnightBSD: src/share/mk/bsd.port.mk,v 1.2 2006/05/22 06:03:21 laffer1 Exp $

PORTSDIR?=	/usr/mports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk

.include <bsd.own.mk>
.include "${BSDPORTMK}"
