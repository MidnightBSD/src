.include <src.opts.mk>

PACKAGE=lib${LIB}
MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libmport

LIB=	mport
SRCS=	asset.c bundle_write.c bundle_read.c plist.c create_primative.c db.c \
        audit.c util.c error.c \
        info.c install_primative.c instance.c \
	version_cmp.c check_preconditions.c delete_primative.c \
	default_cbs.c  merge_primative.c bundle_read_install_pkg.c \
	update_primative.c bundle_read_update_pkg.c pkgmeta.c \
    	fetch.c index.c index_depends.c install.c clean.c setting.c  \
   	stats.c verify.c lock.c mkdir.c \
	import_export.c upgrade.c update.c autoremove.c ping.c
INCS=	mport.h

CFLAGS+=	-I${.CURDIR} -I${MPORTDIR}/libmport
CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include
SHLIB_MAJOR=	2
MAN=	mport.3

WARNS=	3

MK_PROFILE=no

LIBADD=	md archive bz2 lzma z fetch sqlite3 ucl pthread util

.include <bsd.lib.mk>
