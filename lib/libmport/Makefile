# $MidnightBSD: src/lib/libmport/Makefile,v 1.14 2012/01/22 17:08:48 laffer1 Exp $

LIB=		mport
SRCS=		bundle_write.c bundle_read.c plist.c create_primative.c db.c \
		util.c error.c install_primative.c instance.c \
		version_cmp.c check_preconditions.c delete_primative.c \
		default_cbs.c  merge_primative.c bundle_read_install_pkg.c \
		update_primative.c bundle_read_update_pkg.c pkgmeta.c \
		fetch.c index.c install.c clean.c setting.c verify.c
INCS=		mport.h 

CFLAGS+=	-I${.CURDIR} -g
WARNS?=		6
WFORMAT?=	1
SHLIB_MAJOR=	1

MAN=	mport.3

DPADD=	${LIBMD} ${LIBARCHIVE} ${LIBBZP2} ${LIBLZMA} ${LIBZ} ${LIBFETCH} ${LIBSQLITE3}
LDADD=	-lmd -larchive -lbz2 -llzma -lz -lfetch -lsqlite3

.include <bsd.lib.mk>
