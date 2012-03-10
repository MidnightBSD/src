# $FreeBSD: src/share/mk/bsd.snmpmod.mk,v 1.2.2.1 2006/01/25 13:22:58 harti Exp $
# $MidnightBSD: src/share/mk/bsd.snmpmod.mk,v 1.2 2006/05/22 06:03:21 laffer1 Exp $

INCSDIR=	${INCLUDEDIR}/bsnmp

SHLIB_NAME=	snmp_${MOD}.so.${SHLIB_MAJOR}
SRCS+=		${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CLEANFILES+=	${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CFLAGS+=	-I.

${MOD}_oid.h: ${MOD}_tree.def ${EXTRAMIBDEFS} ${EXTRAMIBSYMS}
	cat ${.ALLSRC} | gensnmptree -e ${XSYM} > ${.TARGET}

.ORDER: ${MOD}_tree.c ${MOD}_tree.h
${MOD}_tree.c ${MOD}_tree.h: ${MOD}_tree.def ${EXTRAMIBDEFS}
	cat ${.ALLSRC} | gensnmptree -p ${MOD}_

.if defined(DEFS)
FILESGROUPS+=	DEFS
DEFSDIR=	${SHAREDIR}/snmp/defs
.endif

.if defined(BMIBS)
FILESGROUPS+=	BMIBS
BMIBSDIR=	${SHAREDIR}/snmp/mibs
.endif

.include <bsd.lib.mk>
