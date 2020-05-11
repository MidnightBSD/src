# $FreeBSD: stable/11/secure/caroot/trusted/Makefile 352948 2019-10-02 01:05:29Z kevans $

BINDIR=		/usr/share/certs/trusted

TRUSTED_CERTS!=	ls ${.CURDIR}/*.pem 2> /dev/null || true

FILES+=	 ${TRUSTED_CERTS}

cleancerts:
	@[ -z "${TRUSTED_CERTS}" ] || rm ${TRUSTED_CERTS}

.include <bsd.prog.mk>
