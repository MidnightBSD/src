# $FreeBSD: src/share/mk/bsd.port.mk,v 1.307 2004/07/02 20:47:18 eik Exp $
# $MidnightBSD: src/share/mk/bsd.port.mk,v 1.3 2006/10/01 15:18:11 laffer1 Exp $

PORTSDIR?=	/usr/mports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.mport.mk


.include <bsd.own.mk>
.include "${BSDPORTMK}"
