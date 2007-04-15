# $FreeBSD: src/share/mk/bsd.port.mk,v 1.307 2004/07/02 20:47:18 eik Exp $
# $MidnightBSD: src/share/mk/bsd.port.mk,v 1.5 2007/04/13 03:20:00 ctriv Exp $

PORTSDIR?=	/usr/mports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.mport.mk

.include <bsd.own.mk>
.include "${BSDPORTMK}"
