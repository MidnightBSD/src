.include <src.opts.mk>

PROG=	msearch

CFLAGS+= -I${.CURDIR}/../../lib/libsearch

CSTD=	c99

LIBADD+=	msearch sqlite3 z

.if ${MK_TESTS} != "no"
HAS_TESTS=
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
