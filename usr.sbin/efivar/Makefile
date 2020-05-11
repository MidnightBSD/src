# $FreeBSD: stable/11/usr.sbin/efivar/Makefile 332123 2018-04-06 18:10:38Z kevans $

PROG=	efivar
MAN=	efivar.8

LIBADD= efivar geom

SRCS=	efivar.c efiutil.c

EFIBOOT=${SRCTOP}/stand/efi
CFLAGS+=	-I${EFIBOOT}/include

.include <bsd.prog.mk>
