# $MidnightBSD: src/tools/tools/tinderbox/www/Makefile,v 1.2 2008/03/06 20:23:45 laffer1 Exp $
# $FreeBSD: src/tools/tools/tinderbox/www/Makefile,v 1.6 2004/01/25 19:11:43 des Exp $

CGI		 = index.cgi
DATA		 = tb.css
WWWDIR		?= ${HOME}/public_html
WWWOWN		?= ${USER}
WWWGRP		?= ${USER}

all:

realinstall:
	${INSTALL} -m0755 -o${WWWOWN} -g${WWWGRP} ${.CURDIR}/${CGI} ${WWWDIR}
.for _data in ${DATA}
	${INSTALL} -m0644 -o${WWWOWN} -g${WWWGRP} ${.CURDIR}/${_data} ${WWWDIR}
.endfor

.include <bsd.prog.mk>
