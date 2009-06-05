# $MidnightBSD: src/lib/libmport/Makefile,v 1.6 2008/01/05 22:18:20 ctriv Exp $

LIB=		mport

SRCS=		bundle_write.c bundle_read.c plist.c create_primative.c db.c \
		util.c error.c install_primative.c instance.c \
		version_cmp.c check_preconditions.c delete_primative.c \
		default_cbs.c  merge_primative.c bundle_read_install_pkg.c \
		update_primative.c bundle_read_update_pkg.c pkgmeta.c \
		fetch.c index.c install.c
		
INCS=		mport.h 


CFLAGS+=	-I${.CURDIR} #-DDEBUGGING
WARNS?=	3
WFORMAT?=	1
SHLIB_MAJOR=	1

DPADD=	${LIBSQLITE3} ${LIBMD} ${LIBARCHIVE} ${LIBBZP2} ${LIBZ} ${LIBFETCH}
LDADD=	-lsqlite3 -lmd -larchive -lbz2 -lz -lfetch

.include <bsd.lib.mk>
