#  $FreeBSD: release/10.0.0/sys/boot/arm/at91/bootiic/Makefile 164323 2006-11-16 00:53:28Z imp $

.PATH:	${.CURDIR}/../libat91

P=bootiic
FILES=${P}
SRCS=arm_init.S main.c loader_prompt.c env_vars.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

CFLAGS += -DBOOT_IIC -DBOOT_COMMANDS
