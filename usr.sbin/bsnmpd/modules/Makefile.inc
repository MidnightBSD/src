# $FreeBSD: release/7.0.0/usr.sbin/bsnmpd/modules/Makefile.inc 170925 2007-06-18 18:47:54Z rafan $

SHLIB_MAJOR=	5
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
