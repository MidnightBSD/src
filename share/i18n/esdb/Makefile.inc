# $FreeBSD: stable/11/share/i18n/esdb/Makefile.inc 284345 2015-06-13 19:20:56Z sjg $

.include <bsd.own.mk>

.PATH: ${.CURDIR}

ESDBDIR?= /usr/share/i18n/esdb
.if ${MK_STAGING} == "yes"
MKESDB= ${STAGE_HOST_OBJTOP}/usr/bin/mkesdb_static
.endif
MKESDB?= ${.OBJDIR}/../../../usr.bin/mkesdb_static/mkesdb_static
