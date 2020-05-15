#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/11/lib/libc/db/recno/Makefile.inc 262722 2014-03-04 02:19:39Z marcel $

.PATH: ${LIBC_SRCTOP}/db/recno

SRCS+=	rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c rec_search.c \
	rec_seq.c rec_utils.c
