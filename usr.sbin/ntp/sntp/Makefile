# $FreeBSD: release/7.0.0/usr.sbin/ntp/sntp/Makefile 139103 2004-12-21 08:47:35Z ru $

.PATH: ${.CURDIR}/../../../contrib/ntp/sntp

PROG=	sntp
NO_MAN=
SRCS= 	internet.c main.c socket.c timing.c unix.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-DPACKAGE=\"sntp\" -DVERSION=\"1.6\"

DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
