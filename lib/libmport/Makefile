# $MidnightBSD$

LIB=		mport
SRCS=		bundle_write.c bundle_read.c plist.c create_primative.c db.c \
		util.c error.c install_primative.c instance.c \
		version_cmp.c check_preconditions.c delete_primative.c \
		default_cbs.c  merge_primative.c bundle_read_install_pkg.c \
		update_primative.c bundle_read_update_pkg.c pkgmeta.c \
		fetch.c index.c index_depends.c install.c clean.c setting.c  \
		stats.c verify.c lock.c
INCS=		mport.h 

CFLAGS+=	-I${.CURDIR} -fblocks -g
SHLIB_MAJOR=	1
MAN=	mport.3

NO_PROFILE=

DPADD=	${LIBMD} ${LIBARCHIVE} ${LIBBZP2} ${LIBLZMA} ${LIBZ} ${LIBFETCH} \
	${LIBSQLITE3} ${LIBPTHREAD} ${LIBDISPATCH} ${LIBBLOCKSRUNTIME}
LDADD=	-lmd -larchive -lbz2 -llzma -lz -lfetch -lsqlite3 \
	-lpthread -ldispatch -lBlocksRuntime

.include <bsd.lib.mk>
