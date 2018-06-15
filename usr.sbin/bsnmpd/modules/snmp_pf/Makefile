# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/bsnmpd/modules/snmp_pf/Makefile 310903 2016-12-31 10:34:09Z ngie $
#
# Author: Philip Paeps <philip@freebsd.org>

MOD=	pf
SRCS=	pf_snmp.c
CFLAGS+= -DSNMPTREE_TYPES

XSYM=	begemotPf
DEFS=	${MOD}_tree.def
BMIBS=	BEGEMOT-PF-MIB.txt

.include <bsd.snmpmod.mk>
