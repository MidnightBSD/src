# $FreeBSD: release/7.0.0/usr.sbin/asf/Makefile 159720 2006-06-18 11:14:40Z yar $

PROG=	asf
SRCS=	asf.c asf_kld.c asf_kvm.c asf_prog.c
MAN=	asf.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

WARNS?=	4

.include <bsd.prog.mk>
