# $FreeBSD: release/10.0.0/gnu/usr.bin/groff/src/devices/grohtml/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG_CXX=	post-grohtml
SRCS=		post-html.cpp html-table.cpp html-text.cpp output.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NO_MAN=

.include <bsd.prog.mk>
