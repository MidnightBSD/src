# $FreeBSD: release/7.0.0/secure/libexec/Makefile 156813 2006-03-17 18:54:44Z ru $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign
.endif

.include <bsd.subdir.mk>
