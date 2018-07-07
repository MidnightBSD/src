# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/opiekey/Makefile 270120 2014-08-18 02:13:45Z ache $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opiekey

CFLAGS+=-I${.CURDIR}/../../lib/libopie
CFLAGS+=-I${OPIE_DIST}
CFLAGS+=-DINSECURE_OVERRIDE

WARNS?=	0

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

LINKS=	${BINDIR}/opiekey ${BINDIR}/otp-md4
LINKS+=	${BINDIR}/opiekey ${BINDIR}/otp-md5
LINKS+=	${BINDIR}/opiekey ${BINDIR}/otp-sha1

MLINKS=	opiekey.1 otp-md4.1 opiekey.1 otp-md5.1 opiekey.1 otp-sha1.1

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
