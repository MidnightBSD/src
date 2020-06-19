# $FreeBSD: stable/11/usr.sbin/fifolog/fifolog_reader/Makefile 321271 2017-07-20 01:02:30Z ngie $

PROG=	fifolog_reader

CFLAGS+= -I${.CURDIR:H}/lib

MAN=

LIBADD=	fifolog

regress:
	./${PROG} /tmp/fifolog.0
	./${PROG} -t /tmp/fifolog.0
	./${PROG} /tmp/fifolog.1
	./${PROG} -B "00:00" /tmp/fifolog.1
	./${PROG} -T "%y%m%d-%H%M%S" /tmp/fifolog.1
	./${PROG} -T "" /tmp/fifolog.1

.include <bsd.prog.mk>
