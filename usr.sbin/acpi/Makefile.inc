# $Id: Makefile.inc,v 1.1.1.3 2008-11-14 19:15:59 laffer1 Exp $
# $FreeBSD: src/usr.sbin/acpi/Makefile.inc,v 1.9 2005/11/01 22:44:08 jkim Exp $

ACPICA_DIR= ${.CURDIR}/../../../sys/contrib/dev/acpica
CFLAGS+= -I${.CURDIR}/../../../sys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.PATH:	${ACPICA_DIR} ${ACPICA_DIR}/compiler ${ACPICA_DIR}/common
