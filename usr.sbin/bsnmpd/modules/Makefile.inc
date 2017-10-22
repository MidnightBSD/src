# $FreeBSD: stable/9/usr.sbin/bsnmpd/modules/Makefile.inc 204596 2010-03-02 18:44:08Z uqs $

SHLIB_MAJOR=	6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'

.include "../Makefile.inc"
