# $FreeBSD: stable/9/sbin/mount_reiserfs/Makefile 198236 2009-10-19 16:00:24Z ru $

PROG = mount_reiserfs
SRCS = mount_reiserfs.c getmntopts.c
MAN  = mount_reiserfs.8

# mount_reiserfs needs mntopts.h and getmntopts.c from src/sbin/mount/
MOUNT  ?= ${.CURDIR}/../mount
CFLAGS += -I${MOUNT}

.PATH: ${MOUNT}

.include <bsd.prog.mk>
