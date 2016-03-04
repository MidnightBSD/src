# $MidnightBSD$

ETCFILES	 =
ETCFILES	+= default.rc
ETCFILES	+= midnightbsd0.4-build.rc midnightbsd0.4-update.rc
ETCFILES	+= head-build.rc head-update.rc
ETCFILES	+= head-noclang-build.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
