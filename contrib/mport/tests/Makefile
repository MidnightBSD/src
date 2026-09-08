PACKAGE=	mport
TESTSDIR=	${TESTSBASE}/mport
KYUAFILE=	no

CFLAGS+=	-I${.CURDIR}/../libmport \
		-I${.CURDIR}/../external/tllist \
		-I${.CURDIR}/../external/lua/src \
		-I/usr/include/private/ucl \
		-I/usr/include/ucl \
		-I/usr/include/private/zstd \
		-I${LOCALBASE}/include

ATF_TESTS_C+=	mport_confirm_test \
		mport_db_test \
		mport_fetch_test \
		mport_index_test \
		mport_install_test \
		mport_osrelease_test \
		mport_pkgmeta_test \
		mport_precheck_test \
		mport_util_test \
		mport_version_test

LDFLAGS.mport_confirm_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_confirm_test+=	-lmport -latf-c

LDFLAGS.mport_fetch_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_fetch_test+=	-lmport -latf-c

LDFLAGS.mport_db_test+=		-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_db_test+=		-lmport -latf-c -lsqlite3

LDFLAGS.mport_index_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_index_test+=	-lmport -latf-c

LDFLAGS.mport_osrelease_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_osrelease_test+=	-lmport -latf-c

LDFLAGS.mport_pkgmeta_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_pkgmeta_test+=	-lmport -latf-c

LDFLAGS.mport_install_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_install_test+=	-lmport -latf-c

LDFLAGS.mport_precheck_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_precheck_test+=	-lmport -latf-c

LDFLAGS.mport_util_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_util_test+=		-lmport -latf-c

LDFLAGS.mport_version_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_version_test+=	-lmport -latf-c -lsqlite3

mportFILES=	Kyuafile \
		mport_create_test \
		mport_libexec_test \
		mport_cli_test

.include <bsd.test.mk>
