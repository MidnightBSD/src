# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: stable/11/usr.sbin/amd/fixmount/Makefile 275054 2014-11-25 16:57:27Z bapt $

.PATH: ${.CURDIR}/../../../contrib/amd/fixmount \
       ${.CURDIR}/../../../contrib/amd/conf/checkmount

PROG=	fixmount
MAN=	fixmount.8
SRCS=	fixmount.c

#	These would be links created by the GNU-style configure
SRCS+=	checkmount_bsd44.c

LIBADD+=	amu rpcsvc

.include <bsd.prog.mk>
