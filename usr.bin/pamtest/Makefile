# $FreeBSD: release/10.0.0/usr.bin/pamtest/Makefile 236109 2012-05-26 17:10:16Z des $

OPENPAM=	${.CURDIR}/../../contrib/openpam
.PATH: ${OPENPAM}/include ${OPENPAM}/bin/pamtest

PROG= pamtest
SRCS= pamtest.c
DPADD= ${LIBPAM}
LDADD= ${MINUSLPAM}

.include <bsd.prog.mk>
