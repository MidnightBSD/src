#	From: @(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: stable/9/usr.sbin/mount_portalfs/Makefile 166158 2007-01-20 23:24:11Z rodrigc $

PROG=	mount_portalfs
SRCS=	mount_portalfs.c activate.c conf.c cred.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_pipe.c pt_tcp.c pt_tcplisten.c
MAN=	mount_portalfs.8

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+=-I${MOUNT}
WARNS?=	3

.PATH:	${MOUNT}

.include <bsd.prog.mk>
