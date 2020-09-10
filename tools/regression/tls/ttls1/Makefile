# $FreeBSD: stable/11/tools/regression/tls/ttls1/Makefile 264400 2014-04-13 05:21:56Z imp $

PROG=	ttls1
LDFLAGS+= -L../libxx -lxx -Wl,--rpath=${.OBJDIR}/../libxx
LDFLAGS+= -L../libyy -lyy -Wl,--rpath=${.OBJDIR}/../libyy
MAN=
DEBUG_FLAGS= -g

.include <bsd.prog.mk>
