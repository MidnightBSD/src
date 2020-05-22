#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/11/lib/libc/db/hash/Makefile.inc 262722 2014-03-04 02:19:39Z marcel $

.PATH: ${LIBC_SRCTOP}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c
