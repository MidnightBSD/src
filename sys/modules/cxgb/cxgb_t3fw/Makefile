# $FreeBSD: stable/11/sys/modules/cxgb/cxgb_t3fw/Makefile 319182 2017-05-30 04:11:12Z ngie $

CXGB = ${SRCTOP}/sys/dev/cxgb
.PATH: ${CXGB} 

KMOD= cxgb_t3fw
SRCS+= cxgb_t3fw.c
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
