# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/mkcsmapper/Makefile.inc 260096 2013-12-30 20:15:46Z dim $

.include <bsd.compiler.mk>

SRCS+=	lex.l yacc.y
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../mkcsmapper \
	 -I${.CURDIR}/../../lib/libc/iconv \
	 -I${.CURDIR}/../../lib/libiconv_modules/mapper_std
CFLAGS.gcc+= --param max-inline-insns-single=64
