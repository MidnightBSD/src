# $FreeBSD: stable/11/etc/devd/Makefile 298140 2016-04-17 03:45:45Z gjb $

.include <src.opts.mk>

FILEGROUPS=	FILES

.if ${MACHINE} == "powerpc"
FILES+=	apple.conf
.endif

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
.if ${MK_ACPI} != "no"
FILES+=	asus.conf
.endif
.if ${MK_HYPERV} != "no"
FILES+=	hyperv.conf
.endif
.endif

.if ${MK_USB} != "no"
FILES+=	uath.conf ulpt.conf usb.conf
.endif

.if ${MK_ZFS} != "no"
FILES+=	zfs.conf
.endif

NO_OBJ=
FILESDIR=	/etc/devd
FILESMODE=	644

.include <bsd.prog.mk>
