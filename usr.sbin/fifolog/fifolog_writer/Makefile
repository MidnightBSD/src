# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/fifolog/fifolog_writer/Makefile 321272 2017-07-20 01:02:37Z ngie $

PROG=	fifolog_writer

CFLAGS+= -I${.CURDIR:H}/lib

MAN=

DPADD=	${LIBFIFOLOG} ${LIBUTIL} ${LIBZ}
LDADD=	${LIBFIFOLOG} -lutil -lz

regress:
	date | ./${PROG} -z 0 /tmp/fifolog.0
	lptest 65 | ./${PROG} -z 9 /tmp/fifolog.1

.include <bsd.prog.mk>
