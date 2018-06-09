#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/10/lib/libc/db/hash/Makefile.inc 107052 2002-11-18 09:50:57Z ru $
# $MidnightBSD$

.PATH: ${.CURDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c
