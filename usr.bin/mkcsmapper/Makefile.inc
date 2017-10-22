# $FreeBSD: stable/9/usr.bin/mkcsmapper/Makefile.inc 243196 2012-11-17 23:14:31Z dim $

.include <bsd.compiler.mk>

SRCS+=	lex.l yacc.y
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../mkcsmapper \
	 -I${.CURDIR}/../../lib/libc/iconv \
	 -I${.CURDIR}/../../lib/libiconv_modules/mapper_std
.if ${COMPILER_TYPE} == "gcc"
CFLAGS+= --param max-inline-insns-single=64
.endif
