# $FreeBSD: release/10.0.0/usr.sbin/apmd/Makefile 208289 2010-05-19 08:52:51Z uqs $

PROG=	apmd
MAN=	apmd.8
MANSUBDIR= /i386
SRCS=	apmd.c apmdlex.l apmdparse.y y.tab.h

WARNS?=	3

DPADD=	${LIBL}
LDADD=	-ll

CFLAGS+= -I${.CURDIR}

test:
	./apmd -d -f etc/apmd.conf -n

.include <bsd.prog.mk>
