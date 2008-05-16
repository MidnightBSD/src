# $MidnightBSD: src/secure/usr.bin/Makefile,v 1.2 2006/10/10 18:52:37 laffer1 Exp $
# $FreeBSD: src/secure/usr.bin/Makefile,v 1.24 2003/07/16 20:59:15 markm Exp $

SUBDIR=
.if !defined(NO_OPENSSL)
SUBDIR+=bdes openssl
.if !defined(NO_OPENSSH)
SUBDIR+=scp sftp ssh ssh-add ssh-agent ssh-keygen ssh-keyscan ssh-vulnkey
.endif
.endif

.include <bsd.subdir.mk>
