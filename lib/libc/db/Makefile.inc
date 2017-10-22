#	from @(#)Makefile.inc	8.2 (Berkeley) 2/21/94
# $FreeBSD: release/10.0.0/lib/libc/db/Makefile.inc 156613 2006-03-13 01:15:01Z deischen $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${.CURDIR}/db/btree/Makefile.inc"
.include "${.CURDIR}/db/db/Makefile.inc"
.include "${.CURDIR}/db/hash/Makefile.inc"
.include "${.CURDIR}/db/man/Makefile.inc"
.include "${.CURDIR}/db/mpool/Makefile.inc"
.include "${.CURDIR}/db/recno/Makefile.inc"

SYM_MAPS+=${.CURDIR}/db/Symbol.map
