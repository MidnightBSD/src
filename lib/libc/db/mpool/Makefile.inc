#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/11/lib/libc/db/mpool/Makefile.inc 296915 2016-03-15 19:26:32Z emaste $

.PATH: ${LIBC_SRCTOP}/db/mpool

SRCS+=	mpool.c
.if ${MK_SYMVER} == yes
SRCS+=	mpool-compat.c
.endif
