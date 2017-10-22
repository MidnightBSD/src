# $FreeBSD: release/10.0.0/usr.sbin/eeprom/Makefile 201390 2010-01-02 11:07:44Z ed $

.PATH: ${.CURDIR}/../ofwdump

PROG=	eeprom
MAN=	eeprom.8
MANSUBDIR= /sparc64
SRCS=	eeprom.c ofw_options.c ofw_util.c
CFLAGS+= -I${.CURDIR}/../ofwdump

.include <bsd.prog.mk>
