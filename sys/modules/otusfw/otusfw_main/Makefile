# $FreeBSD: stable/11/sys/modules/otusfw/otusfw_main/Makefile 319182 2017-05-30 04:11:12Z ngie $

KMOD=	otusfw_main
FIRMWS=	otusfw_main:otusfw_main:1

CLEANFILES=	otusfw_main

otusfw_main: ${SRCTOP}/sys/contrib/dev/otus/otus-main
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.kmod.mk>
