#	from: @(#)bsd.subdir.mk	5.9 (Berkeley) 2/1/91
# $FreeBSD: src/share/mk/bsd.subdir.mk,v 1.50 2005/05/31 07:14:51 ru Exp $
#
# The include file <bsd.subdir.mk> contains the default targets
# for building subdirectories.
#
# For all of the directories listed in the variable SUBDIRS, the
# specified directory will be visited and the target made. There is
# also a default target which allows the command "make subdir" where
# subdir is any directory listed in the variable SUBDIRS.
#
#
# +++ variables +++
#
# DISTRIBUTION	Name of distribution. [base]
#
# SUBDIR	A list of subdirectories that should be built as well.
#		Each of the targets will execute the same target in the
#		subdirectories.
#
# +++ targets +++
#
#	distribute:
# 		This is a variant of install, which will
# 		put the stuff into the right "distribution".
#
#	afterinstall, all, all-man, beforeinstall, checkdpadd,
#	clean, cleandepend, cleandir, depend, install, lint, maninstall,
#	obj, objlink, realinstall, regress, tags
#

.include <bsd.init.mk>

DISTRIBUTION?=	base
.if !target(distribute)
distribute:
.for dist in ${DISTRIBUTION}
	${_+_}cd ${.CURDIR}; \
	    ${MAKE} install -DNO_SUBDIR DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
.endif

_SUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR) && !defined(NO_SUBDIR)
	@${_+_}for entry in ${SUBDIR}; do \
		if test -d ${.CURDIR}/$${entry}.${MACHINE_ARCH}; then \
			${ECHODIR} "===> ${DIRPRFX}$${entry}.${MACHINE_ARCH} (${.TARGET:realinstall=install})"; \
			edir=$${entry}.${MACHINE_ARCH}; \
			cd ${.CURDIR}/$${edir}; \
		else \
			${ECHODIR} "===> ${DIRPRFX}$$entry (${.TARGET:realinstall=install})"; \
			edir=$${entry}; \
			cd ${.CURDIR}/$${edir}; \
		fi; \
		${MAKE} ${.TARGET:realinstall=install} \
		    DIRPRFX=${DIRPRFX}$$edir/; \
	done
.endif

${SUBDIR}: .PHONY
	${_+_}@if test -d ${.TARGET}.${MACHINE_ARCH}; then \
		cd ${.CURDIR}/${.TARGET}.${MACHINE_ARCH}; \
	else \
		cd ${.CURDIR}/${.TARGET}; \
	fi; \
	${MAKE} all


.for __target in all all-man checkdpadd clean cleandepend cleandir \
    depend distribute lint maninstall \
    obj objlink realinstall regress tags \
    ${SUBDIR_TARGETS}
${__target}: _SUBDIR
.endfor

.for __target in files includes
.for __stage in build install
${__stage}${__target}:
.if make(${__stage}${__target})
${__stage}${__target}: _SUBDIR
.endif
.endfor
${__target}:
	${_+_}cd ${.CURDIR}; ${MAKE} build${__target}; ${MAKE} install${__target}
.endfor

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif
install: beforeinstall realinstall afterinstall
.ORDER: beforeinstall realinstall afterinstall
.endif
