# $FreeBSD: release/7.0.0/sys/modules/harp/Makefile 125771 2004-02-13 10:11:01Z ru $
#
# Author: Harti Brandt <brandt@fokus.gmd.de>
#
.PATH: ${.CURDIR}/../../dev/harp

KMOD=	if_harp
SRCS=	if_harp.c device_if.h

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../include
CFLAGS+= -DDEBUG -DINVARIANTS

.include <bsd.kmod.mk>
