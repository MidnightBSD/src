# $MidnightBSD$
# $FreeBSD: stable/10/kerberos5/libexec/Makefile 311730 2017-01-09 01:31:12Z ngie $

.include <bsd.own.mk>

SUBDIR=	digest-service ipropd-master ipropd-slave hprop hpropd kdc \
	kdigest kfd kimpersonate kpasswdd kcm
SUBDIR_PARALLEL=

.if ${MK_GSSAPI} != "no"
SUBDIR+=	kadmind
.endif

.include <bsd.subdir.mk>
