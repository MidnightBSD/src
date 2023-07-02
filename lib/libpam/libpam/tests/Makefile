# $FreeBSD: stable/10/lib/libpam/libpam/tests/Makefile 313488 2017-02-09 22:49:48Z ngie $

OPENPAM=	${SRCTOP}/contrib/openpam
.PATH: ${OPENPAM}/t

TESTSDIR = ${TESTSBASE}/lib/libpam

COMMONSRC = t_file.c t_main.c
.for test in t_openpam_ctype t_openpam_readlinev t_openpam_readword
TAP_TESTS_C += ${test}
SRCS.${test} = ${test}.c ${COMMONSRC}
.endfor
CFLAGS +=-I${OPENPAM}/include -I${OPENPAM}/lib/libpam -I${OPENPAM}/t
WARNS ?= 6

DPADD = ${LIBPAM}
LDADD = ${MINUSLPAM}

.include <bsd.test.mk>
