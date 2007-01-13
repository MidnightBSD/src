# $MidnightBSD: src/etc/isdn/Makefile,v 1.2 2006/08/28 01:35:54 laffer1 Exp $
# $FreeBSD: src/etc/isdn/Makefile,v 1.13 2004/12/21 08:46:50 ru Exp $

NO_OBJ=

SCRIPTS=	answer			\
		isdntel.sh		\
		record			\
		tell			\
		tell-record		\
		unknown_incoming

FILES=		holidays.D		\
		isdnd.rates.A		\
		isdnd.rates.D		\
		isdnd.rates.F		\
		isdnd.rates.L		\
		isdnd.rates.UK.BT	\
		isdnd.rc.sample		\
		isdntel.alias.sample

BINDIR=		/etc/isdn
BINMODE=	700
FILESMODE=	600

.include <bsd.prog.mk>
