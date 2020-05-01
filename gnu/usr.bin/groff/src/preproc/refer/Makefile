# $FreeBSD: stable/11/gnu/usr.bin/groff/src/preproc/refer/Makefile 291738 2015-12-04 03:17:47Z bdrewery $

PROG_CXX=	refer
SRCS=		command.cpp label.cpp ref.cpp refer.cpp token.cpp
CFLAGS+=	-I${DIST_DIR}
DPADD=		${LIBBIB} ${LIBGROFF}
LDADD=		${LIBBIB} ${LIBGROFF}
LIBADD=		m
CLEANFILES=	label.cpp label_tab.h ${MAN} y.tab.c y.tab.h

.include <bsd.prog.mk>
