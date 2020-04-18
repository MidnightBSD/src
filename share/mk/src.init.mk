# $FreeBSD: stable/11/share/mk/src.init.mk 290467 2015-11-06 21:49:17Z bdrewery $

.if !target(__<src.init.mk>__)
__<src.init.mk>__:

.if !target(buildenv)
buildenv: .PHONY
	${_+_}@env BUILDENV_DIR=${.CURDIR} ${MAKE} -C ${SRCTOP} buildenv
.endif

.endif	# !target(__<src.init.mk>__)
