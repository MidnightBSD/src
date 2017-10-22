#
# $FreeBSD: release/10.0.0/usr.sbin/bsnmpd/modules/snmp_wlan/Makefile 222772 2011-06-06 20:24:17Z ed $
#

MOD=	wlan
SRCS=	wlan_snmp.c wlan_sys.c
CFLAGS+= -DSNMPTREE_TYPES

XSYM=	begemotWlan

BMIBS=	BEGEMOT-WIRELESS-MIB.txt
MAN=	snmp_${MOD}.3
DEFS=	${MOD}_tree.def

.include <bsd.snmpmod.mk>
