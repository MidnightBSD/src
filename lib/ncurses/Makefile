# $MidnightBSD$
# $FreeBSD: stable/10/lib/ncurses/Makefile 276678 2015-01-05 00:15:27Z ngie $

.include <bsd.own.mk>

SUBDIR=	ncurses form menu panel

.if ${MK_NCURSESW} != "no"
SUBDIR+=	ncursesw formw menuw panelw
.endif

SUBDIR_PARALLEL=

.for subdir in ${SUBDIR:Nncurses*:N*w}
SUBDIR_DEPEND_${subdir}=	ncurses
SUBDIR_DEPEND_${subdir}w=	ncursesw
.endfor

.include <bsd.subdir.mk>
