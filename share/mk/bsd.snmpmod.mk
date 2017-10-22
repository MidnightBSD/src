# $FreeBSD: release/7.0.0/share/mk/bsd.snmpmod.mk 152277 2005-11-10 12:07:12Z harti $

INCSDIR=	${INCLUDEDIR}/bsnmp

SHLIB_NAME=	snmp_${MOD}.so.${SHLIB_MAJOR}
SRCS+=		${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CLEANFILES+=	${MOD}_oid.h ${MOD}_tree.c ${MOD}_tree.h
CFLAGS+=	-I.

${MOD}_oid.h: ${MOD}_tree.def ${EXTRAMIBDEFS}
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
