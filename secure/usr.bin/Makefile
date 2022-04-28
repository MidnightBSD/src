# $FreeBSD: stable/11/secure/usr.bin/Makefile 289393 2015-10-15 22:55:08Z bdrewery $

.include <src.opts.mk>

SUBDIR=
.if ${MK_OPENSSL} != "no"
SUBDIR+=bdes openssl netpgp netpgpkeys
.if ${MK_OPENSSH} != "no"
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan
.endif
.endif

SUBDIR.${MK_TESTS}+= tests

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
