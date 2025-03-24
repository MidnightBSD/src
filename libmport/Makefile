PACKAGE=lib${LIB}

LIB=	mport
SRCS=	asset.c bundle_write.c bundle_read.c plist.c create_primative.c db.c \
        util.c error.c \
        info.c install_primative.c instance.c \
		version_cmp.c check_preconditions.c delete_primative.c \
		default_cbs.c  merge_primative.c bundle_read_install_pkg.c \
		update_primative.c bundle_read_update_pkg.c pkgmeta.c \
    	fetch.c index.c index_depends.c install.c clean.c setting.c  \
   		stats.c update.c upgrade.c verify.c lock.c mkdir.c import_export.c \
   		autoremove.c audit.c ping.c message.c service.c list.c
INCS=	mport.h

CFLAGS+=	-I${.CURDIR} -I ../external/tllist/ -I/usr/include/private/ucl -I/usr/include/ucl -I/usr/include/private/zstd
SHLIB_MAJOR=	2
MAN=	mport.3

WARNS=	3

MK_PROFILE=no

LIBADD=	md archive lzma z fetch sqlite3 ucl pthread util zstd

LDFLAGS+=	-lmd -larchive -llzma -lz -lfetch -lsqlite3 -lpthread -lprivateucl -lutil -lprivatezstd

.include <bsd.lib.mk>
