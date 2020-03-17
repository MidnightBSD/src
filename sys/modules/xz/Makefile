# $FreeBSD: stable/11/sys/modules/xz/Makefile 344605 2019-02-26 19:55:03Z kib $

.PATH:	${SRCTOP}/sys/dev/xz
.PATH:	${SRCTOP}/sys/contrib/xz-embedded/freebsd
.PATH:	${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz

KMOD=	xz
SRCS= \
	xz_crc32.c \
	xz_dec_bcj.c \
	xz_dec_lzma2.c \
	xz_dec_stream.c \
	xz_mod.c

CFLAGS+= \
	-I${SRCTOP}/sys/contrib/xz-embedded/freebsd \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/include/linux

.include <bsd.kmod.mk>
