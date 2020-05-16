# $FreeBSD: stable/11/share/i18n/csmapper/Makefile.inc 284345 2015-06-13 19:20:56Z sjg $

.include <bsd.own.mk>

CSMAPPERDIR?= /usr/share/i18n/csmapper
.if ${MK_STAGING} == "yes"
MKCSMAPPER= ${STAGE_HOST_OBJTOP}/usr/bin/mkcsmapper_static
.endif
MKCSMAPPER?= ${.OBJDIR}/../../../usr.bin/mkcsmapper_static/mkcsmapper_static
