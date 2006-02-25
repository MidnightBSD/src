# $Id: Makefile.inc,v 1.1.1.1 2006-02-25 02:29:33 laffer1 Exp $
# $FreeBSD: src/usr.sbin/acpi/Makefile.inc,v 1.8 2003/08/07 16:51:50 njl Exp $

ACPICA_DIR= ${.CURDIR}/../../../sys/contrib/dev/acpica
CFLAGS+= -I${.CURDIR}/../../../sys -I${ACPICA_DIR} -I${ACPICA_DIR}/compiler

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.PATH:	${ACPICA_DIR} ${ACPICA_DIR}/compiler ${ACPICA_DIR}/common
