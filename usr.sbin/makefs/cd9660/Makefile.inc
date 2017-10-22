#	$FreeBSD: release/10.0.0/usr.sbin/makefs/cd9660/Makefile.inc 214921 2010-11-07 16:05:04Z cognet $
#

.PATH:	${.CURDIR}/cd9660 ${.CURDIR}/../../sys/fs/cd9660/

CFLAGS+=-I${.CURDIR}/../../sys/fs/cd9660/

SRCS+=	cd9660_strings.c cd9660_debug.c cd9660_eltorito.c \
	cd9660_write.c cd9660_conversion.c iso9660_rrip.c cd9660_archimedes.c
