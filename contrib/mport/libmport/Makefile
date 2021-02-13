PACKAGE=lib${LIB}

LIB=	mport
SRCS=	asset.c bundle_write.c bundle_read.c plist.c create_primative.c db.c \
        dispatch.c util.c error.c \
        info.c install_primative.c instance.c \
		version_cmp.c check_preconditions.c delete_primative.c \
		default_cbs.c  merge_primative.c bundle_read_install_pkg.c \
		update_primative.c bundle_read_update_pkg.c pkgmeta.c \
    	fetch.c index.c index_depends.c install.c clean.c setting.c  \
   		stats.c verify.c lock.c mkdir.c hashmap.c
INCS=	mport.h mport_dispatch.h

CFLAGS+=	-I${.CURDIR} -fblocks -g
SHLIB_MAJOR=	2
MAN=	mport.3

WARNS=	3

MK_PROFILE=no

#DPADD=	${LIBMD} ${LIBARCHIVE} ${LIBBZP2} ${LIBLZMA} ${LIBZ} ${LIBFETCH} \
#	${LIBSQLITE3} ${LIBPTHREAD} ${LIBDISPATCH} ${LIBBLOCKSRUNTIME}
#LDADD=	-lmd -larchive -lbz2 -llzma -lz -lfetch -lsqlite3 \
#	-lpthread -ldispatch -lBlocksRuntime

LIBADD=	md archive bz2 lzma z fetch sqlite3 dispatch BlocksRuntime pthread

LDFLAGS+=	-lmd -larchive -lbz2 -llzma -lz -lfetch -lsqlite3 -ldispatch -lBlocksRuntime -lpthread

.include <bsd.lib.mk>
