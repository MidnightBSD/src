# $FreeBSD: release/10.0.0/sys/boot/arm/at91/Makefile.inc 251250 2013-06-02 11:44:23Z tijl $

.if !target(__at91_boot_Makefile.inc__)
.PATH:	${.CURDIR}/../../../../libkern ${.CURDIR}/../../../../libkern/arm

__at91_boot_Makefile.inc__:

# tsc, bwct, kb920x, centipad are the supported flavors
BOOT_FLAVOR?=kb920x

CFLAGS=-Os -mcpu=arm9 -ffreestanding \
	-I${.CURDIR}/../libat91 \
	-I${.CURDIR}/../../../.. \
	-I${.CURDIR}/../../../../arm \
	-D_KERNEL \
	-Wall -Waggregate-return  \
	-Wnested-externs \
	-Wpointer-arith -Wshadow -Wwrite-strings \
	-Werror \
	-Wmissing-prototypes \
	-Wmissing-declarations 
# -Wstrict-prototypes 

CFLAGS+=-DBOOT_${BOOT_FLAVOR:tu}

LIBAT91=${.OBJDIR}/../libat91/libat91.a

LD ?= ld
OBJCOPY ?= objcopy

.if defined(P)
${P}:	${P}.out
	${OBJCOPY} -S -O binary ${P}.out ${.TARGET}
	@set -- `ls -l ${.TARGET}`; x=$$((12288-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0

${P}.out: ${OBJS}
	${LD} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LIBAT91}

CLEANFILES+= ${P} ${P}.out
.endif

.if defined(WITH_TAG_LIST)
MK_TAG_LIST:=yes
.else
MK_TAG_LIST:=no
.endif

.if defined(WITH_FPGA)
MK_FPGA:=yes
.else
MK_FPGA:=no
.endif

.endif

.include "../Makefile.inc"
