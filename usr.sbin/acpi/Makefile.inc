# $Id: Makefile.inc,v 1.2 2012-06-25 02:46:24 laffer1 Exp $
# $FreeBSD: src/usr.sbin/acpi/Makefile.inc,v 1.9 2005/11/01 22:44:08 jkim Exp $
# MidnightBSD$

CFLAGS+=-fno-strict-aliasing

ACPICA_DIR= ${.CURDIR}/../../../sys/contrib/dev/acpica
CFLAGS+= -I${.CURDIR}/../../../sys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.PATH:	${ACPICA_DIR} ${ACPICA_DIR}/compiler ${ACPICA_DIR}/common
