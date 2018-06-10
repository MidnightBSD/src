# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/fifolog/fifolog_reader/Makefile 321272 2017-07-20 01:02:37Z ngie $

PROG=	fifolog_reader

CFLAGS+= -I${.CURDIR:H}/lib

MAN=

DPADD=	${LIBFIFOLOG} ${LIBUTIL} ${LIBZ}
LDADD=	${LIBFIFOLOG} -lutil -lz

regress:
	./${PROG} /tmp/fifolog.0
	./${PROG} -t /tmp/fifolog.0
	./${PROG} /tmp/fifolog.1
	./${PROG} -B "00:00" /tmp/fifolog.1
	./${PROG} -T "%y%m%d-%H%M%S" /tmp/fifolog.1
	./${PROG} -T "" /tmp/fifolog.1

.include <bsd.prog.mk>
