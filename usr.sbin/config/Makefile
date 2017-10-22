# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/9/usr.sbin/config/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	config
MAN=	config.5 config.8
SRCS=	config.y main.c lang.l mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h kernconf.c

kernconf.c: kernconf.tmpl
	file2c 'char kernconfstr[] = {' ',0};' < ${.CURDIR}/kernconf.tmpl > kernconf.c

CFLAGS+= -I. -I${.CURDIR}

DPADD=	${LIBL} ${LIBSBUF}
LDADD=	-ll -lsbuf

CLEANFILES+=	kernconf.c

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
