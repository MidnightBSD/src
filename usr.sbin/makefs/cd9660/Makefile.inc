#	$FreeBSD: stable/11/usr.sbin/makefs/cd9660/Makefile.inc 330976 2018-03-15 09:04:23Z eadler $
#

.PATH:	${SRCDIR}/cd9660 ${SRCTOP}/sys/fs/cd9660/

CFLAGS+=-I${SRCTOP}/sys/fs/cd9660/

SRCS+=	cd9660_strings.c cd9660_debug.c cd9660_eltorito.c \
	cd9660_write.c cd9660_conversion.c iso9660_rrip.c cd9660_archimedes.c
