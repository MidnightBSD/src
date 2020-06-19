# $FreeBSD: stable/11/usr.sbin/eeprom/Makefile 319119 2017-05-29 10:15:41Z ngie $

.PATH: ${.CURDIR:H}/ofwdump

PROG=	eeprom
MAN=	eeprom.8
MANSUBDIR= /sparc64
SRCS=	eeprom.c ofw_options.c ofw_util.c
CFLAGS+= -I${.CURDIR:H}/ofwdump

.include <bsd.prog.mk>
