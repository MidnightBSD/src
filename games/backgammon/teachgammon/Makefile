#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $DragonFly: src/games/backgammon/teachgammon/Makefile,v 1.3 2006/10/08 16:22:35 pavalos Exp $
# $MidnightBSD: src/games/backgammon/teachgammon/Makefile,v 1.2 2009/04/11 16:52:49 laffer1 Exp $

PROG=	teachgammon
CFLAGS+=-DTEACHGAMMON_TEXT -DV7 -I${.CURDIR}/../common_source
SRCS=	allow.c board.c check.c data.c fancy.c init.c odds.c one.c save.c \
	subs.c table.c teach.c ttext1.c ttext2.c tutor.c
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
HIDEGAME=hidegame
NO_MAN=	noman

WARNS?=	0

.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
