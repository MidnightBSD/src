#	$FreeBSD: src/usr.sbin/makefs/ffs/Makefile.inc,v 1.1.4.2 2011/07/26 14:41:54 marius Exp $
#

.PATH:	${.CURDIR}/ffs ${.CURDIR}/../../sys/ufs/ffs

CFLAGS+=	-I${.CURDIR}/../../sys/ufs/ffs

SRCS+=	ffs_alloc.c ffs_balloc.c ffs_bswap.c ffs_subr.c ufs_bmap.c
SRCS+=	buf.c mkfs.c
