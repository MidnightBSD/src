# $FreeBSD: stable/11/secure/caroot/Makefile 357192 2020-01-28 02:58:39Z kevans $

PACKAGE=	caroot

CLEANFILES+=	certdata.txt

SUBDIR+=	trusted
SUBDIR+=	blacklisted

.include <bsd.obj.mk>

# To be used by secteam@ to update the trusted certificates
fetchcerts: .PHONY
	fetch --no-sslv3 --no-tlsv1 -o certdata.txt 'https://hg.mozilla.org/projects/nss/raw-file/tip/lib/ckfw/builtins/certdata.txt'

cleancerts: .PHONY
	@${MAKE} -C ${.CURDIR}/trusted ${.TARGET}

updatecerts: .PHONY cleancerts fetchcerts
	perl ${.CURDIR}/MAca-bundle.pl -i certdata.txt -o ${.CURDIR}/trusted
