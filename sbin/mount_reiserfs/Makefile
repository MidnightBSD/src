# $FreeBSD: release/7.0.0/sbin/mount_reiserfs/Makefile 152358 2005-11-13 01:47:04Z rodrigc $

PROG = mount_reiserfs
SRCS = mount_reiserfs.c getmntopts.c
MAN  = mount_reiserfs.8

# mount_reiserfs needs mntopts.h and getmntopts.c from src/sbin/mount/
MOUNT  ?= ${.CURDIR}/../mount
CFLAGS += -I${MOUNT}
WARNS ?= 6

.PATH: ${MOUNT}

.include <bsd.prog.mk>
