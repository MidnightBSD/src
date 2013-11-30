# $Id: Makefile,v 1.2 2008-11-26 15:43:57 laffer1 Exp $
# $FreeBSD: src/sys/modules/dcons/Makefile,v 1.5 2007/06/01 00:23:34 simokawa Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons
SRCS	= dcons.c dcons.h dcons_os.c dcons_os.h \
		opt_dcons.h opt_kdb.h opt_ddb.h opt_gdb.h opt_comconsole.h

.if !defined(KERNBUILDDIR)
opt_kdb.h:
	echo "#define KDB 1" > $@

opt_ddb.h:
	echo "#define DDB 1" > $@

opt_comconsole.h:
	echo "#define ALT_BREAK_TO_DEBUGGER 1" > $@
.endif

CFLAGS+= -I${.CURDIR}/../.. 

.include <bsd.kmod.mk>
