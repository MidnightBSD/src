# $FreeBSD: release/10.0.0/secure/libexec/Makefile 204949 2010-03-10 11:33:15Z des $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign ssh-pkcs11-helper
.endif

.include <bsd.subdir.mk>
