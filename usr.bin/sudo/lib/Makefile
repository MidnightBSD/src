#	$OpenBSD: Makefile,v 1.3 2005/06/21 02:11:43 brad Exp $
#	$MidnightBSD: src/usr.bin/sudo/lib/Makefile,v 1.1 2007/02/04 17:33:53 archite Exp $

SUDO_DIR=	${.CURDIR}/../../../contrib/sudo
.PATH:		${SUDO_DIR}

LIB=		sudo
CFLAGS+=	-I. -I${SUDO_DIR}
SRCS=		alloc.c defaults.c fileops.c find_path.c gettime.c goodpath.c \
		lex.yy.c sudo.tab.c
CLEANFILES+=	lex.yy.c sudo.tab.c sudo.tab.h
NO_PROFILE= 	yes

.if !defined(NO_PIC)
CPPFLAGS+= -D_PATH_SUDO_NOEXEC=\"/usr/libexec/sudo_noexec.so\"
NO_PIC=	yes
.endif

sudo.tab.c sudo.tab.h: parse.yacc
	${YACC} -d -b sudo ${SUDO_DIR}/parse.yacc

lex.yy.c: parse.lex sudo.tab.c sudo.tab.h
	${LEX} ${SUDO_DIR}/parse.lex

.include <bsd.lib.mk>
