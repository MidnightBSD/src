# $FreeBSD: src/share/mk/bsd.port.mk,v 1.307 2004/07/02 20:47:18 eik Exp $
# $MidnightBSD: src/share/mk/bsd.port.mk,v 1.3 2006/10/01 15:18:11 laffer1 Exp $

PORTSDIR?=	/usr/mports

.if defined(PORT_SYSTEM) && ${PORT_SYSTEM:L} == mport
BSDPORTMK?=     ${PORTSDIR}/Mk/bsd.mport.mk
.else
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.port.mk
.endif

.include <bsd.own.mk>
.include "${BSDPORTMK}"
