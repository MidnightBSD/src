
PACKAGE=runtime
SCRIPTS = midnightbsd-version
MAN = midnightbsd-version.1
CLEANFILES = midnightbsd-version midnightbsd-version.sh
NEWVERS = ${SRCTOP}/sys/conf/newvers.sh

midnightbsd-version.sh: ${.CURDIR}/midnightbsd-version.sh.in ${NEWVERS}
	eval $$(egrep '^(TYPE|REVISION|BRANCH)=' ${NEWVERS}) ; \
	if ! sed -e "\
			s/@@TYPE@@/$${TYPE}/g; \
			s/@@REVISION@@/$${REVISION}/g; \
			s/@@BRANCH@@/$${BRANCH}/g; \
		" ${.CURDIR}/midnightbsd-version.sh.in >${.TARGET} ; then \
		rm -f ${.TARGET} ; \
		exit 1 ; \
	fi

.include <bsd.prog.mk>
