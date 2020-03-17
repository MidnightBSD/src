# $FreeBSD: stable/11/sys/modules/otusfw/otusfw_init/Makefile 319182 2017-05-30 04:11:12Z ngie $

KMOD=	otusfw_init
FIRMWS=	otusfw_init:otusfw_init:1

CLEANFILES=	otus_init

otusfw_init: ${SRCTOP}/sys/contrib/dev/otus/otus-init
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.kmod.mk>
