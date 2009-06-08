# $MidnightBSD: src/secure/lib/Makefile,v 1.2 2008/11/06 21:47:15 laffer1 Exp $
# $FreeBSD: src/secure/lib/Makefile,v 1.34 2006/03/17 18:54:31 ru Exp $

.include <bsd.own.mk>

SUBDIR= 
.if ${MK_OPENSSL} != "no"
SUBDIR+=libcrypto libssl libnetpgp
.if ${MK_OPENSSH} != "no"
SUBDIR+=libssh
.endif
.endif

.include <bsd.subdir.mk>
