#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/lib/libc/regex/Makefile.inc 156613 2006-03-13 01:15:01Z deischen $

# regex sources
.PATH: ${.CURDIR}/regex

CFLAGS+=-DPOSIX_MISTAKE

SRCS+=	regcomp.c regerror.c regexec.c regfree.c

SYM_MAPS+=${.CURDIR}/regex/Symbol.map

MAN+=	regex.3
MAN+=	re_format.7

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3
MLINKS+=regexec.3 regfree.3
