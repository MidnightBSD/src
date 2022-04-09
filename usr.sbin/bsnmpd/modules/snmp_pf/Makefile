# $FreeBSD: stable/11/usr.sbin/bsnmpd/modules/snmp_pf/Makefile 310901 2016-12-31 10:32:49Z ngie $
#
# Author: Philip Paeps <philip@freebsd.org>

MOD=	pf
SRCS=	pf_snmp.c
CFLAGS+= -DSNMPTREE_TYPES

XSYM=	begemotPf
DEFS=	${MOD}_tree.def
BMIBS=	BEGEMOT-PF-MIB.txt

.include <bsd.snmpmod.mk>
