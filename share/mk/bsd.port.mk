# $FreeBSD: src/share/mk/bsd.port.mk,v 1.307 2004/07/02 20:47:18 eik Exp $
# $MidnightBSD: src/share/mk/bsd.port.mk,v 1.7 2008/10/14 21:13:54 laffer1 Exp $

PORTSDIR?=	/usr/mports
BSDPORTMK?=	${PORTSDIR}/Mk/bsd.mport.mk

# Needed to keep bsd.own.mk from reading in /etc/src.conf
# and setting MK_* variables when building ports.
_WITHOUT_SRCCONF=

# Enable CTF conversion on request.
.if defined(WITH_CTF)
.undef NO_CTF
.endif

.include <bsd.own.mk>
.include "${BSDPORTMK}"
