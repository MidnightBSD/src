.include <src.opts.mk>

PACKAGE=lib${LIB}
MPORTDIR= ${SRCTOP}/contrib/mport
.PATH: ${MPORTDIR}/libmport

ZSTDDIR=        ${SRCTOP}/sys/contrib/zstd

LIB=	mport
SRCS=	asset.c bundle_write.c bundle_read.c plist.c create_primative.c db.c \
        util.c error.c \
        info.c install_primative.c instance.c \
		version_cmp.c check_preconditions.c delete_primative.c \
		default_cbs.c  merge_primative.c bundle_read_install_pkg.c \
		update_primative.c bundle_read_update_pkg.c pkgmeta.c \
    	fetch.c index.c index_depends.c install.c clean.c setting.c  \
   		stats.c verify.c lock.c mkdir.c \
		import_export.c upgrade.c update.c audit.c autoremove.c ping.c message.c \
		service.c list.c \
		lua.c lua_scripts.c
.PATH: ${MPORTDIR}/external/tllist
INCS=	mport.h tllist.h

.PATH: ${MPORTDIR}/external/lua/src
SRCS+=	lapi.c \
	lcode.c \
	lctype.c \
	ldebug.c \
	ldo.c \
	ldump.c \
	lfunc.c \
	lgc.c \
	llex.c \
	lmem.c \
	lobject.c \
	lopcodes.c \
	lparser.c \
	lstate.c \
	lstring.c \
	ltable.c \
	ltm.c \
	lundump.c \
	lvm.c \
	lzio.c \
	lauxlib.c \
	lbaselib.c \
	lcorolib.c \
	ldblib.c \
	liolib.c \
	lmathlib.c \
	loslib.c \
	lstrlib.c \
	ltablib.c \
	lutf8lib.c \
	loadlib.c \
	linit.c

CFLAGS+=	-I${.CURDIR} -I${MPORTDIR}/libmport -I${MPORTDIR}/external/tllist/ -I${MPORTDIR}/external/lua/src/
CFLAGS+=-I${SRCTOP}/contrib/libucl/include
.PATH:  ${SRCTOP}/contrib/libucl/include
CFLAGS+= -I${ZSTDDIR}/lib
.PATH: ${ZSTDDIR}/lib
SHLIB_MAJOR=	2
MAN=	mport.3

WARNS=	3

MK_PROFILE=no

LIBADD=	md archive lzma z elf fetch sqlite3 ucl pthread util zstd

.include <bsd.lib.mk>
