# $FreeBSD: release/10.0.0/sys/modules/netgraph/atm/sscfu/Makefile 121462 2003-10-24 07:42:08Z harti $
#
# Author: Harti Brandt <harti@freebsd.org>
#

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm/sscfu ${LIBBASE}/netnatm/saal

KMOD=	ng_sscfu
SRCS= 	ng_sscfu.c saal_sscfu.c
CFLAGS+= -I${LIBBASE} # -DSSCFU_DEBUG

.include <bsd.kmod.mk>
