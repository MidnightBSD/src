#	$FreeBSD: release/10.0.0/share/examples/kld/firmware/fwimage/Makefile 220256 2011-04-01 23:47:10Z emaste $

KMOD=	beastie
FIRMWS=	firmware.img:beastie

CLEANFILES=	firmware.img

firmware.img: firmware.img.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>
