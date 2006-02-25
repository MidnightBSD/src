# $Id: Makefile,v 1.1.1.1 2006-02-25 02:28:24 laffer1 Exp $
# $FreeBSD: src/sys/modules/dcons/Makefile,v 1.3 2004/10/13 05:41:09 simokawa Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons
SRCS	= dcons.c dcons.h dcons_os.c dcons_os.h \
		opt_dcons.h opt_ddb.h opt_comconsole.h

opt_ddb.h:
	echo "#define KDB 1" > $@

opt_comconsole.h:
	echo "#define ALT_BREAK_TO_DEBUGGER 1" > $@

CFLAGS+= -I${.CURDIR}/../.. 

.include <bsd.kmod.mk>
