#  $FreeBSD: stable/9/sys/boot/arm/at91/boot2/Makefile 164320 2006-11-16 00:48:04Z imp $

.PATH: ${.CURDIR}/../libat91 ${.CURDIR}/../bootspi

P=boot2
FILES=${P}
SRCS=arm_init.S boot2.c ${BOOT_FLAVOR:L}_board.c
NO_MAN=
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
