
CLEANFILES+=	certdata.txt

SUBDIR+=	trusted
SUBDIR+=	blacklisted

.include <bsd.obj.mk>

# To be used by secteam@ to update the trusted certificates
fetchcerts: .PHONY
	fetch --no-sslv3 --no-tlsv1 -o certdata.txt 'https://raw.githubusercontent.com/mozilla-firefox/firefox/refs/heads/release/security/nss/lib/ckfw/builtins/certdata.txt'

cleancerts: .PHONY
	@${MAKE} -C ${.CURDIR}/trusted ${.TARGET}

updatecerts: .PHONY cleancerts fetchcerts
	perl ${.CURDIR}/MAca-bundle.pl -i certdata.txt -o ${.CURDIR}/trusted
