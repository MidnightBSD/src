
ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/cmd/zdb
.PATH: ${ZFSTOP}/man/man8

PACKAGE=	zfs
PROG=	zdb
MAN=	zdb.8
INCS=	zdb.h
SRCS=	zdb.c zdb_il.c

WARNS?=	2
CSTD=	c99

CFLAGS+= \
	-DIN_BASE \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd/spl \
	-I${SRCTOP}/sys/contrib/openzfs/lib/libzpool/include \
	-I${SRCTOP}/sys \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID

LIBADD=	nvpair umem uutil zfs spl avl zutil zpool crypto

CFLAGS.gcc+= -fms-extensions
# Since there are many asserts in this program, it makes no sense to compile
# it without debugging.
CFLAGS+=	-g -DDEBUG=1 -DZFS_DEBUG=1

.include <bsd.compiler.mk>

# dump_l2arc_header() declares l2arc_dev_hdr_phys_t locals, which have
# const members, without an initializer.
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 210000
CWARNFLAGS+=	-Wno-default-const-init-field-unsafe
.endif

.include <bsd.prog.mk>
