#	$FreeBSD: stable/11/share/examples/kld/firmware/fwimage/Makefile 299293 2016-05-10 00:51:50Z gjb $

PACKAGE=examples
FILESDIR=${SHAREDIR}/examples/kld/fwimage
KMOD=	beastie
FIRMWS=	firmware.img:beastie

CLEANFILES=	firmware.img

firmware.img: firmware.img.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>
