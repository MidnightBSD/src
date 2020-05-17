#	$NetBSD: Makefile.inc,v 1.8 2002/02/04 00:18:32 thorpej Exp $
# $FreeBSD: stable/11/usr.bin/xlint/Makefile.inc 279907 2015-03-12 08:32:24Z imp $

WARNS?=		0

.PATH:		${.CURDIR}/../common

.if exists(${.CURDIR}/../arch/${MACHINE_ARCH})
CFLAGS+=	-I${.CURDIR}/../arch/${MACHINE_ARCH}
.else
CFLAGS+=	-I${.CURDIR}/../arch/${MACHINE_CPUARCH}
.endif
CFLAGS+=	-I${.CURDIR}/../common

OBJECT_FMT=	ELF
