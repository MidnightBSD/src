#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/hash/Makefile.inc,v 1.5 2002/11/18 09:50:54 ru Exp $
# $MidnightBSD: src/lib/libc/db/hash/Makefile.inc,v 1.2 2009/03/16 16:23:37 laffer1 Exp $

.PATH: ${.CURDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c
