#	pccardc Makefile
#
# $FreeBSD: release/7.0.0/usr.sbin/pccard/dumpcis/Makefile 163491 2006-10-18 21:09:43Z imp $

.PATH: ${.CURDIR}/../pccardd ${.CURDIR}/../pccardc

PROG=	dumpcis
MAN=	dumpcis.8
SRCS=	main.c dumpcis.c dumpcisfile.c readcis.c printcis.c

CFLAGS+= -I${.CURDIR}/../pccardd -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
