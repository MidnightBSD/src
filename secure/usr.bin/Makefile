# $MidnightBSD: src/secure/usr.bin/Makefile,v 1.3 2008/05/16 20:36:39 laffer1 Exp $
# $FreeBSD: src/secure/usr.bin/Makefile,v 1.24 2003/07/16 20:59:15 markm Exp $

.include <bsd.own.mk>

SUBDIR=
.if ${MK_OPENSSL} != "no"
SUBDIR+=bdes openssl
.if ${MK_OPENSSH} != "no"
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan ssh-vulnkey
.endif
.endif

.include <bsd.subdir.mk>
