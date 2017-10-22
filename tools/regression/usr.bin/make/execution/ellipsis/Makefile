# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/execution/ellipsis/Makefile 228149 2011-11-30 05:49:17Z fjoe $

ELLIPSIS=	...

check-ellipsis:
	@${MAKE} -f ${MAKEFILE} do-$@
	@${MAKE} -f ${MAKEFILE} -j2 do-$@
	@${MAKE} -f ${MAKEFILE} -j2 -B do-$@

do-check-ellipsis: do-check-ellipsis-1 do-check-ellipsis-2
.ORDER: do-check-ellipsis-1 do-check-ellipsis-2

do-check-ellipsis-1:
	@echo before $@
	${ELLIPSIS}
	@echo after $@

do-check-ellipsis-2:
	@echo before $@
	${ELLIPSIS}
	@echo after $@
