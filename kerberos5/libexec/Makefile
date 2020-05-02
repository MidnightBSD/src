# $FreeBSD: stable/11/kerberos5/libexec/Makefile 311731 2017-01-09 01:31:16Z ngie $

.include <src.opts.mk>

SUBDIR=	digest-service ipropd-master ipropd-slave hprop hpropd kdc \
	kdigest kfd kimpersonate kpasswdd kcm
SUBDIR_PARALLEL=

.if ${MK_GSSAPI} != "no"
SUBDIR+=	kadmind
.endif

.include <bsd.subdir.mk>
