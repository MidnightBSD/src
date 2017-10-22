# $FreeBSD: release/10.0.0/tools/tools/ath/athalq/Makefile 250266 2013-05-05 09:38:25Z adrian $

PROG=	athalq
NOMAN=	yes

CFLAGS+=	-I../../../../sys/contrib

SRCS=	main.c ar5210_ds.c ar5211_ds.c ar5212_ds.c ar5416_ds.c tdma.c
SRCS+=	ar9300_ds.c

.include <../Makefile.inc>

.include <bsd.prog.mk>
