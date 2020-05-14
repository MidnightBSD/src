# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: stable/11/usr.sbin/amd/hlfsd/Makefile 275054 2014-11-25 16:57:27Z bapt $

.PATH: ${.CURDIR}/../../../contrib/amd/hlfsd

PROG=	hlfsd
MAN=	hlfsd.8
SRCS=	hlfsd.c homedir.c nfs_prot_svc.c stubs.c

CFLAGS+= -I${.CURDIR}/../../../contrib/amd/hlfsd

LIBADD=	amu

.include <bsd.prog.mk>
