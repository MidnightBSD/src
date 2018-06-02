# $MidnightBSD$
#  $FreeBSD: stable/10/sys/boot/arm/at91/boot2/Makefile 276486 2014-12-31 23:25:37Z ngie $

.PATH: ${.CURDIR}/../libat91 ${.CURDIR}/../bootspi

P=boot2
FILES=${P}
SRCS=arm_init.S boot2.c ${BOOT_FLAVOR:tl}_board.c
MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

.if ${BOOT_FLAVOR} == "kb920x"
CFLAGS+=-DBOOT_IIC
.endif
CFLAGS+= \
	-I${.CURDIR}/../bootspi \
	-I${.CURDIR}/../../../common \
	-I${.CURDIR}/../../../.. \
	-D_KERNEL \
	-DUFS1_ONLY
