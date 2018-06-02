# $MidnightBSD$
#  $FreeBSD: stable/10/sys/boot/arm/at91/bootiic/Makefile 276486 2014-12-31 23:25:37Z ngie $

.PATH:	${.CURDIR}/../libat91

P=bootiic
FILES=${P}
SRCS=arm_init.S main.c loader_prompt.c env_vars.c
MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

CFLAGS += -DBOOT_IIC -DBOOT_COMMANDS
