# $FreeBSD: stable/11/sys/modules/random_fortuna/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/random

KMOD	= random_fortuna
SRCS	= randomdev.c hash.c fortuna.c
SRCS	+= opt_param.h bus_if.h device_if.h
SRCS	+= opt_ddb.h
CFLAGS	+= -DRANDOM_LOADABLE

.include <bsd.kmod.mk>
