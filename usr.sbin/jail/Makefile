# $FreeBSD: release/10.0.0/usr.sbin/jail/Makefile 249657 2013-04-19 19:45:00Z ed $

.include <bsd.own.mk>

PROG=	jail
MAN=	jail.8 jail.conf.5
SRCS=	jail.c command.c config.c state.c jailp.h jaillex.l jailparse.y y.tab.h

DPADD=	${LIBJAIL} ${LIBKVM} ${LIBUTIL} ${LIBL}
LDADD=	-ljail -lkvm -lutil -ll

NO_WMISSING_VARIABLE_DECLARATIONS=

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif
.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif

CLEANFILES= y.output

.include <bsd.prog.mk>
