#	$FreeBSD: release/10.0.0/tools/tools/ath/Makefile.inc 252207 2013-06-25 17:50:48Z adrian $

BINDIR=	/usr/local/bin
NO_MAN=

ATH_DEFAULT=	ath0

CFLAGS+=-DATH_DEFAULT='"${ATH_DEFAULT}"'
CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../common
CFLAGS+=-I${.CURDIR}/../../../../sys
CFLAGS+=-I${.CURDIR}/../../../../sys/dev/ath
CFLAGS+=-I${.CURDIR}/../../../../sys/dev/ath/ath_hal
CFLAGS+=-I${.CURDIR}/../../../../sys/contrib/dev/ath/ath_hal
CFLAGS+=-I${.OBJDIR}
