# $MidnightBSD: src/sys/boot/i386/Makefile,v 1.3 2011/10/16 21:11:08 laffer1 Exp $

.include <bsd.own.mk>

SUBDIR=		mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot kgzldr \
		libi386 libfirewire loader

# special boot programs, 'self-extracting boot2+loader'
SUBDIR+=	pxeldr

.if ${MK_ZFS} != "no"
SUBDIR+=	zfsboot gptzfsboot zfsloader
.endif

.include <bsd.subdir.mk>
