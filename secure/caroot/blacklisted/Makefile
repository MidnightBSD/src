# $FreeBSD: stable/11/secure/caroot/blacklisted/Makefile 357633 2020-02-06 18:37:38Z kevans $

BINDIR=		/usr/share/certs/blacklisted

BLACKLISTED_CERTS!=	ls ${.CURDIR}/*.pem 2> /dev/null || true

FILES+=	 ${BLACKLISTED_CERTS}

.include <bsd.prog.mk>
