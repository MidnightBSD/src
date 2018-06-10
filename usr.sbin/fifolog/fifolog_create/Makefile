# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/fifolog/fifolog_create/Makefile 321272 2017-07-20 01:02:37Z ngie $

PROG=	fifolog_create

CFLAGS+= -I${.CURDIR:H}/lib

DPADD=	${LIBFIFOLOG} ${LIBUTIL}
LDADD=	${LIBFIFOLOG} -lutil

MAN=	fifolog.1
MLINKS=	fifolog.1 fifolog_create.1 \
	fifolog.1 fifolog_reader.1 \
	fifolog.1 fifolog_writer.1

regress:
	rm -f /tmp/fifolog.?
	./${PROG} /tmp/fifolog.0
	./${PROG} -s 10m /tmp/fifolog.1
	./${PROG} -l 1k /tmp/fifolog.2
	./${PROG} -r 1k /tmp/fifolog.3

.include <bsd.prog.mk>
