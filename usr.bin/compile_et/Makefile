# $FreeBSD: release/10.0.0/usr.bin/compile_et/Makefile 233294 2012-03-22 08:48:42Z stas $

.PATH: ${.CURDIR}/../../contrib/com_err

LIBVERS=	${.OBJDIR}/../../kerberos5/lib/libvers/libvers.a

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l
LDADD=	-lroken ${LIBVERS}
DPADD=	${LIBROKEN} ${LIBVERS}
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

WARNS?=	0

.include <bsd.prog.mk>
