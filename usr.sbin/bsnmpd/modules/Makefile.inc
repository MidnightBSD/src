# $FreeBSD: stable/11/usr.sbin/bsnmpd/modules/Makefile.inc 298107 2016-04-16 07:45:30Z gjb $

SHLIB_MAJOR=	6
PACKAGE=	bsnmp

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'

NO_WMISSING_VARIABLE_DECLARATIONS=

.include "../Makefile.inc"
