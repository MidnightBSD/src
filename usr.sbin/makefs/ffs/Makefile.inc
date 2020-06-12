#	$FreeBSD: stable/11/usr.sbin/makefs/ffs/Makefile.inc 330976 2018-03-15 09:04:23Z eadler $
#

.PATH:	${SRCDIR}/ffs ${SRCTOP}/sys/ufs/ffs

CFLAGS+=	-I${SRCTOP}/sys/ufs/ffs

SRCS+=	ffs_alloc.c ffs_balloc.c ffs_bswap.c ffs_subr.c ufs_bmap.c
SRCS+=	buf.c mkfs.c
