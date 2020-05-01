# $FreeBSD: stable/11/lib/ncurses/Makefile 276320 2014-12-27 21:11:42Z ngie $

SUBDIR=	ncurses form menu panel \
	ncursesw formw menuw panelw

SUBDIR_PARALLEL=

.for subdir in ${SUBDIR:Nncurses*:N*w}
SUBDIR_DEPEND_${subdir}=	ncurses
SUBDIR_DEPEND_${subdir}w=	ncursesw
.endfor

.include <bsd.subdir.mk>
