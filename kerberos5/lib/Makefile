# $MidnightBSD$
# $FreeBSD: stable/10/kerberos5/lib/Makefile 311729 2017-01-09 01:29:20Z ngie $

.include <bsd.own.mk>

SUBDIR=	libasn1 libhdb \
	libheimntlm libhx509 libkadm5clnt libkadm5srv libkafs5 libkrb5 \
	libroken libsl libvers libkdc libwind libheimsqlite libheimbase libheimipcc libheimipcs

SUBDIR_DEPEND_libkafs5=	libkrb5

.if ${MK_GSSAPI} != "no"
SUBDIR+=	libgssapi_krb5
SUBDIR+=	libgssapi_ntlm
SUBDIR+=	libgssapi_spnego
.endif

.include <bsd.subdir.mk>
