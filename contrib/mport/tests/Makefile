PACKAGE=	mport
TESTSDIR=	${TESTSBASE}/mport
KYUAFILE=	no

ATF_TESTS_C+=	mport_fetch_test
LDFLAGS.mport_fetch_test+=	-L${.CURDIR}/../libmport \
				-L${LOCALBASE}/lib \
				-Wl,-rpath,${.CURDIR}/../libmport \
				-Wl,-rpath,${LOCALBASE}/lib
LDADD.mport_fetch_test+=	-lmport -latf-c

mportFILES=	Kyuafile \
		mport_create_test \
		mport_libexec_test \
		mport_cli_test

.include <bsd.test.mk>
