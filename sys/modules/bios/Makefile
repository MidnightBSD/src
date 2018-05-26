# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/bios/Makefile 309842 2016-12-11 02:01:59Z rpokala $
#

SUBDIR=	smbios vpd

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	smapi
.endif

.include <bsd.subdir.mk>
