
.PATH: ${.CURDIR:H}

SRCS=	zfs_defs.c
OBJS=	zfs_defs.o
WARNS?=	1

CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/head
CFLAGS+= -I${.CURDIR:H}
CFLAGS+= -DNEED_SOLARIS_BOOLEAN
CFLAGS+= -fno-builtin -nostdlib

all: ${OBJS}
CLEANFILES= ${OBJS}

.include <bsd.lib.mk>
