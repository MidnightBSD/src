# $FreeBSD: stable/11/usr.sbin/fstyp/Makefile 339118 2018-10-03 02:52:47Z mav $

.include <src.opts.mk>

PROG=	fstyp
SRCS=	cd9660.c exfat.c ext2fs.c fstyp.c geli.c msdosfs.c ntfs.c ufs.c

.if ${MK_ZFS} != "no"
SRCS +=	zfs.c
.endif

MAN=	fstyp.8

WARNS?=	2


.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.if ${MK_ZFS} != "no"
IGNORE_PRAGMA=  YES

CFLAGS+= -DNEED_SOLARIS_BOOLEAN -DHAVE_ZFS
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/head
.endif

CFLAGS+=-I${SRCTOP}/sys

LIBADD=	geom md

.if ${MK_ZFS} != "no"
LIBADD+=nvpair zfs
.endif

.include <bsd.prog.mk>
