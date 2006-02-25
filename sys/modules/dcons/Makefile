# $Id: Makefile,v 1.1.1.2 2006-02-25 02:37:21 laffer1 Exp $
# $FreeBSD: src/sys/modules/dcons/Makefile,v 1.3.2.1 2006/02/10 00:38:00 yar Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons
SRCS	= dcons.c dcons.h dcons_os.c dcons_os.h \
		opt_dcons.h opt_ddb.h opt_comconsole.h

.if !defined(KERNBUILDDIR)
opt_ddb.h:
	echo "#define KDB 1" > $@

opt_comconsole.h:
	echo "#define ALT_BREAK_TO_DEBUGGER 1" > $@
.endif

CFLAGS+= -I${.CURDIR}/../.. 

.include <bsd.kmod.mk>
