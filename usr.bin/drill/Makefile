# $FreeBSD: stable/11/usr.bin/drill/Makefile 319186 2017-05-30 04:27:05Z ngie $

# Vendor sources and generated files
LDNSDIR= ${SRCTOP}/contrib/ldns

.PATH: ${LDNSDIR}/drill

PROG=	drill
SRCS=	drill.c drill_util.c error.c root.c work.c \
	chasetrace.c dnssec.c securetrace.c
CFLAGS+= -I${LDNSDIR}
NO_WERROR= true
MAN=	drill.1

LIBADD=	ldns crypto

.include <bsd.prog.mk>
