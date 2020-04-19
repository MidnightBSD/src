# $FreeBSD: stable/11/stand/sparc64/boot1/Makefile 329183 2018-02-12 20:51:28Z kevans $

.include <bsd.init.mk>

PROG=	boot1.elf
INTERNALPROG=
FILES?=	boot1
SRCS=	_start.s boot1.c
CLEANFILES+=${FILES} boot1.aout

BOOTBLOCKBASE= 0x4000

CFLAGS.clang+=-mcmodel=small
CFLAGS.gcc+=-mcmodel=medlow
CFLAGS+=-Os -I${LDRSRC}
LDFLAGS+=-Ttext ${BOOTBLOCKBASE} -Wl,-N

# Construct boot1. sunlabel expects it to contain zeroed-out space for the
# label, and to be of the correct size.
${FILES}: boot1.aout
	@set -- `ls -l ${.ALLSRC}`; x=$$((7680-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0
	${DD} if=/dev/zero of=${.TARGET} bs=512 count=16
	${DD} if=${.ALLSRC} of=${.TARGET} bs=512 oseek=1 conv=notrunc

boot1.aout: boot1.elf
	elf2aout -o ${.TARGET} ${.ALLSRC}

.include <bsd.prog.mk>
