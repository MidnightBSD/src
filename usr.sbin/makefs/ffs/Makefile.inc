#	$FreeBSD: src/usr.sbin/makefs/ffs/Makefile.inc,v 1.1 2010/11/07 16:05:04 cognet Exp $
#

.PATH:	${.CURDIR}/ffs ${.CURDIR}/../../sys/ufs/ffs

CFLAGS+=	-I${.CURDIR}/../../sys/ufs/ffs

SRCS+=	ffs_alloc.c ffs_balloc.c ffs_bswap.c ffs_subr.c ufs_bmap.c
SRCS+=	buf.c mkfs.c
