# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/pc98/boot0/Makefile 281289 2015-04-09 06:38:32Z dim $

PROG=	${BOOT}.out
INTERNALPROG=
FILES=	${BOOT}
MAN=
SRCS=	${BOOT}.s
CLEANFILES= ${BOOT}

BOOT=	boot0

# The base address that we the boot0 code to to run it.  Don't change this
# unless you are glutton for punishment.
BOOT_BOOT0_ORG?=	0x0000

LDFLAGS=-e start -Ttext ${BOOT_BOOT0_ORG} -Wl,-N

${BOOT}: ${BOOT}.out
	${OBJCOPY} -S -O binary ${BOOT}.out ${.TARGET}

.include <bsd.prog.mk>
