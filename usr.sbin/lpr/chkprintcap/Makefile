# $FreeBSD: release/10.0.0/usr.sbin/lpr/chkprintcap/Makefile 98702 2002-06-23 20:57:18Z gad $

.PATH: ${.CURDIR}/../common_source

PROG=	chkprintcap
MAN=	chkprintcap.8
SRCS=	chkprintcap.c skimprintcap.c

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
