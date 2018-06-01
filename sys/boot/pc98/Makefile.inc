# $MidnightBSD$
# Common defines for all of /sys/boot/pc98/
#
# $FreeBSD: stable/10/sys/boot/pc98/Makefile.inc 260096 2013-12-30 20:15:46Z dim $

BINDIR?=	/boot

LOADER_ADDRESS?=0x200000
CFLAGS+=	-march=i386 -ffreestanding
CFLAGS.gcc+=	-mpreferred-stack-boundary=2
CFLAGS+=	-mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3 -msoft-float
CFLAGS+=	-Os -DPC98
LDFLAGS+=	-nostdlib

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o

.include "../Makefile.inc"
