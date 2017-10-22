# $FreeBSD: release/10.0.0/usr.sbin/asf/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	asf
SRCS=	asf.c asf_kld.c asf_kvm.c asf_prog.c
MAN=	asf.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
