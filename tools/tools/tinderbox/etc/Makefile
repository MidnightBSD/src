# $MidnightBSD: src/tools/tools/tinderbox/etc/Makefile,v 1.2 2008/03/06 20:23:45 laffer1 Exp $

ETCFILES	 =
ETCFILES	+= default.rc
ETCFILES	+= midnightbsd0.4-build.rc midnightbsd0.4-update.rc
ETCFILES	+= head-build.rc head-update.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
