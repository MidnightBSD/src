# $FreeBSD: stable/11/sys/modules/netgraph/atm/sscop/Makefile 319182 2017-05-30 04:11:12Z ngie $
#
# Author: Harti Brandt <harti@freebsd.org>

LIBBASE= ${SRCTOP}/sys/contrib/ngatm

.PATH: ${SRCTOP}/sys/netgraph/atm/sscop ${LIBBASE}/netnatm/saal

KMOD=	ng_sscop
SRCS= 	ng_sscop.c saal_sscop.c
CFLAGS+= -I${LIBBASE} # -DSSCOP_DEBUG

.include <bsd.kmod.mk>
