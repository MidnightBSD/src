# $FreeBSD: stable/9/usr.bin/mkesdb/Makefile.inc 229083 2011-12-31 13:13:42Z gabor $

SRCS+=	lex.l yacc.y
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../mkesdb \
	 -I${.CURDIR}/../../lib/libc/iconv
