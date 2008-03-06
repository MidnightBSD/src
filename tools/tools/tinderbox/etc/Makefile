# $MidnightBSD: src/tools/tools/tinderbox/etc/Makefile,v 1.1.1.1.2.1 2008/03/04 03:06:59 laffer1 Exp $
# $FreeBSD: src/tools/tools/tinderbox/etc/Makefile,v 1.9 2004/08/19 07:28:30 des Exp $

ETCFILES	 =
ETCFILES	+= default.rc
ETCFILES	+= releng_0_1.rc update_releng_0_1.rc
ETCFILES	+= current.rc update_current.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
