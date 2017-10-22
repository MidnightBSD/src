# $FreeBSD: release/7.0.0/share/examples/isdn/i4brunppp/Makefile 135814 2004-09-26 09:25:03Z ru $

PROG	 =	i4brunppp
SRCS	 =	i4brunppp.c
MAN	 =	i4brunppp.8

RUNPPPDEST=	/etc/isdn

INSTALL	?=	install

install:
	@echo i4brunppp is not installed automatically. In case you want to run
	@echo user-ppp in dial-in server under i4b, please copy it into /etc/isdn.

installetc:	i4brunppp
	${INSTALL} -m ${BINMODE} -o ${BINOWN} -g ${BINGRP} \
		${.CURDIR}/i4brunppp ${RUNPPPDEST}

.include <bsd.prog.mk>
