#	from @(#)Makefile.inc	8.2 (Berkeley) 2/21/94
# $FreeBSD: stable/11/lib/libc/db/Makefile.inc 262722 2014-03-04 02:19:39Z marcel $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${LIBC_SRCTOP}/db/btree/Makefile.inc"
.include "${LIBC_SRCTOP}/db/db/Makefile.inc"
.include "${LIBC_SRCTOP}/db/hash/Makefile.inc"
.include "${LIBC_SRCTOP}/db/man/Makefile.inc"
.include "${LIBC_SRCTOP}/db/mpool/Makefile.inc"
.include "${LIBC_SRCTOP}/db/recno/Makefile.inc"

SYM_MAPS+=${LIBC_SRCTOP}/db/Symbol.map
