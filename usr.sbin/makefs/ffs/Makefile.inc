#	$FreeBSD: release/10.0.0/usr.sbin/makefs/ffs/Makefile.inc 214921 2010-11-07 16:05:04Z cognet $
#

.PATH:	${.CURDIR}/ffs ${.CURDIR}/../../sys/ufs/ffs

CFLAGS+=	-I${.CURDIR}/../../sys/ufs/ffs

SRCS+=	ffs_alloc.c ffs_balloc.c ffs_bswap.c ffs_subr.c ufs_bmap.c
SRCS+=	buf.c mkfs.c
