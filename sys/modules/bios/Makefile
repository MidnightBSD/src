# $FreeBSD: stable/11/sys/modules/bios/Makefile 309841 2016-12-11 02:01:57Z rpokala $
#

SUBDIR=	smbios vpd

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	smapi
.endif

.include <bsd.subdir.mk>
