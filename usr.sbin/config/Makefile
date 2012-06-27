# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/config/Makefile,v 1.38 2007/10/01 18:15:11 ru Exp $
# $MidnightBSD: src/usr.sbin/config/Makefile,v 1.3 2008/09/30 04:44:37 laffer1 Exp $

PROG=	config
MAN=	config.5 config.8
SRCS=	config.y main.c lang.l mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h kernconf.c

CSTD=	c90

kernconf.c: kernconf.tmpl
	file2c 'char kernconfstr[] = {' ',0};' < ${.CURDIR}/kernconf.tmpl > kernconf.c

WARNS?=	6
CFLAGS+= -I. -I${.CURDIR}

DPADD=	${LIBL} ${LIBSBUF}
LDADD=	-ll -lsbuf

CLEANFILES+=	kernconf.c

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
