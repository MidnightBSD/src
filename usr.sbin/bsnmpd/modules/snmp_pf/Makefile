# $FreeBSD: release/7.0.0/usr.sbin/bsnmpd/modules/snmp_pf/Makefile 152269 2005-11-10 10:14:57Z harti $
# 
# Author: Philip Paeps <philip@freebsd.org>

MOD=	pf
SRCS=	pf_snmp.c
WARNS?=	6

XSYM=	begemotPf
DEFS=	${MOD}_tree.def
BMIBS=	BEGEMOT-PF-MIB.txt

.include <bsd.snmpmod.mk>
