# $Id: Makefile,v 1.6 2003/10/24 15:41:26 simokawa Exp $
# $FreeBSD: release/10.0.0/sys/modules/dcons/Makefile 225214 2011-08-27 14:24:27Z rwatson $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons
SRCS	= dcons.c dcons.h dcons_os.c dcons_os.h \
		opt_dcons.h opt_kdb.h opt_ddb.h opt_gdb.h

.if !defined(KERNBUILDDIR)
opt_kdb.h:
	echo "#define KDB 1" > $@

opt_ddb.h:
	echo "#define DDB 1" > $@
.endif

CFLAGS+= -I${.CURDIR}/../.. 

.include <bsd.kmod.mk>
