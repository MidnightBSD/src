#	from @(#)Makefile.inc	8.2 (Berkeley) 7/14/94
# $FreeBSD: stable/11/lib/libc/db/btree/Makefile.inc 262722 2014-03-04 02:19:39Z marcel $

.PATH: ${LIBC_SRCTOP}/db/btree

SRCS+=	bt_close.c bt_conv.c bt_debug.c bt_delete.c bt_get.c bt_open.c \
	bt_overflow.c bt_page.c bt_put.c bt_search.c bt_seq.c bt_split.c \
	bt_utils.c
