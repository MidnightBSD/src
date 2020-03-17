# $FreeBSD: stable/11/sys/modules/vnic/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

CFLAGS+=	-DFDT

SUBDIR = mrmlbus thunder_mdio thunder_bgx vnicpf vnicvf

.include <bsd.subdir.mk>
