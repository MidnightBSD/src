# $MidnightBSD$
# $FreeBSD: stable/10/secure/libexec/Makefile 291792 2015-12-04 18:14:31Z bdrewery $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSH} != "no"
SUBDIR+=sftp-server ssh-keysign ssh-pkcs11-helper
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=tests
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
