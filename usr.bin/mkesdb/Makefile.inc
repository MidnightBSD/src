# $FreeBSD: stable/10/usr.bin/mkesdb/Makefile.inc 228766 2011-12-21 12:07:14Z gabor $

SRCS+=	lex.l yacc.y
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../mkesdb \
	 -I${.CURDIR}/../../lib/libc/iconv
