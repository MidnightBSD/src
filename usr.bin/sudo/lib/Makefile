#	$OpenBSD: Makefile,v 1.3 2005/06/21 02:11:43 brad Exp $
#	$MidnightBSD: src/usr.bin/sudo/lib/Makefile,v 1.4 2009/03/04 23:47:32 laffer1 Exp $

SUDO_DIR=	${.CURDIR}/../../../contrib/sudo
.PATH:		${SUDO_DIR}

LIB=		sudo
CFLAGS+=	-I. -I${SUDO_DIR}
SRCS=		alias.c alloc.c closefrom.c defaults.c error.c fileops.c \
		find_path.c gettime.c goodpath.c list.c \
		match.c redblack.c toke.c gram.c zero_bytes.c
CLEANFILES+=	gram.c gram.h toke.c
NO_PROFILE= 	yes

.if !defined(NO_PIC)
CPPFLAGS+= -D_PATH_SUDO_NOEXEC=\"/usr/libexec/sudo_noexec.so\"
NO_PIC=	yes
.endif

gram.c gram.h: gram.y
	rm -f gram.c gram.h
	${YACC} -d ${SUDO_DIR}/gram.y
	mv -f y.tab.c gram.c
	if cmp -s y.tab.h ../gram.h; then rm -f y.tab.h; else mv -f y.tab.h ../gram.h; fi

toke.c: toke.l
	rm -f toke.c
	${LEX} ${SUDO_DIR}/toke.l
	mv -f lex.yy.c toke.c

beforedepend: gram.c toke.c

.include <bsd.lib.mk>
