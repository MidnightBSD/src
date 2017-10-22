#	$FreeBSD: stable/9/tools/tools/ath/Makefile.inc 217681 2011-01-21 02:56:59Z adrian $

BINDIR=	/usr/local/bin
NO_MAN=

ATH_DEFAULT=	ath0

CFLAGS+=-DATH_DEFAULT='"${ATH_DEFAULT}"'
CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../common
CFLAGS+=-I${.CURDIR}/../../../../sys
CFLAGS+=-I${.CURDIR}/../../../../sys/dev/ath
CFLAGS+=-I${.CURDIR}/../../../../sys/dev/ath/ath_hal
CFLAGS+=-I${.OBJDIR}
