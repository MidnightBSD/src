# $FreeBSD: stable/11/usr.sbin/efibootmgr/Makefile 326725 2017-12-09 06:52:20Z imp $
EFIBOOT=${SRCTOP}/stand/efi
EFIINCL=${SRCTOP}/stand/efi/include
EFIVAR=${SRCTOP}/usr.sbin/efivar
.PATH: ${EFIBOOT}/libefi ${EFIVAR}
CFLAGS+= -I${EFIVAR} -I${EFIINCL}

PROG=efibootmgr
MAN=	efibootmgr.8
SRCS= efichar.c efiutil.c efibootmgr.c

LIBADD= efivar geom

.include <bsd.prog.mk>
