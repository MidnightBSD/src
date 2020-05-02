# $FreeBSD: stable/11/lib/atf/common.mk 273929 2014-11-01 11:17:54Z jmmv $
#
# Common Makefile code for all components of ATF.
#

.if !defined(ATF)
.error "ATF must be defined and point to the contrib/atf directory"
.endif

# Depend on the atf-version target to generate a file that contains the
# version number of the currently imported ATF release and that only
# changes on new imports.
atf-version: atf-version-real
	@cmp -s atf-version atf-version-real \
	    || cp atf-version-real atf-version
atf-version-real: .PHONY
	@grep 'define VERSION' ${ATF}/config.h \
	    | cut -d '"' -f 2 >atf-version-real
CLEANFILES+= atf-version atf-version-real
