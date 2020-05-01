# $FreeBSD: stable/11/usr.sbin/unbound/Makefile.inc 356345 2020-01-04 01:09:33Z cy $

NO_WERROR= true
PACKAGE=	unbound

.for man in ${MAN}
${man}: ${UNBOUNDDIR}/doc/${man:S/local-//}
	sed -E \
	    -e 's/\<(fI)?unbound\>/\1local-unbound/g' \
	    -e 's/\<(fI)?Unbound\>/Local-unbound/g' \
	    -e 's/\/local-unbound/\/unbound/g' \
	    <${.ALLSRC} >${.TARGET}
CLEANFILES += ${man}
.endfor

.include "../Makefile.inc"
