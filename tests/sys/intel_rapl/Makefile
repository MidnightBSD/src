
PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/intel_rapl

.PATH:		${SRCTOP}/sys/dev/intel_rapl

ATF_TESTS_C+=	intel_rapl_calc_test
SRCS.intel_rapl_calc_test=	intel_rapl_calc_test.c intel_rapl_calc.c
CFLAGS+=	-I${SRCTOP}/sys/dev/intel_rapl

MAN=

.include <bsd.test.mk>
