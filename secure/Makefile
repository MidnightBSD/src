# $FreeBSD: stable/11/secure/Makefile 357082 2020-01-24 15:29:33Z kevans $

.include <src.opts.mk>

SUBDIR= lib .WAIT \
	libexec ${_tests} usr.bin usr.sbin
SUBDIR_PARALLEL=

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

SUBDIR.${MK_CAROOT}+= caroot

# These are the programs which depend on crypto, but not Kerberos.
SPROGS=	lib/libfetch lib/libmport lib/libpam lib/libradius lib/libtelnet	\
	bin/ed libexec/telnetd usr.bin/fetch usr.bin/perl usr.bin/telnet	\
	usr.sbin/mport usr.sbin/ppp usr.sbin/tcpdump/tcpdump
.if ${MK_SENDMAIL} != "no"
SPROGS+=usr.sbin/sendmail
.endif

# This target is used to rebuild these programs with crypto.
secure: .MAKE .PHONY
.for entry in ${SPROGS}
	cd ${.CURDIR:H}/${entry}; \
	${MAKE} cleandir; \
	${MAKE} obj; \
	${MAKE} all; \
	${MAKE} install
.endfor

# This target is used to rebuild these programs without crypto.
insecure: .MAKE .PHONY
.for entry in ${SPROGS}
	cd ${.CURDIR:H}/${entry}; \
	${MAKE} MK_CRYPT=no cleandir; \
	${MAKE} MK_CRYPT=no obj; \
	${MAKE} MK_CRYPT=no all; \
	${MAKE} MK_CRYPT=no install
.endfor

.include <bsd.subdir.mk>
