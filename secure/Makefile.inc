# $FreeBSD: stable/11/secure/Makefile.inc 319189 2017-05-30 04:42:36Z ngie $

.include <src.opts.mk>

.if exists(${.CURDIR:H:H}/lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR:H:H}/lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR:H:H}/lib/libcrypt
.endif

.if ${MK_OPENSSH} != "no"
SSHDIR=		${SRCTOP}/crypto/openssh
.endif

WARNS?=		0
