# $MidnightBSD$
# $FreeBSD: stable/10/share/mk/netbsd-tests.test.mk 276478 2014-12-31 20:13:31Z ngie $

.if !target(__netbsd_tests.test.mk__)
__netbsd_tests.test.mk__:

.if !defined(OBJTOP)
.error "Please define OBJTOP to the absolute path of the top of the object tree"
.endif

.if !defined(SRCTOP)
.error "Please define SRCTOP to the absolute path of the top of the source tree"
.endif

.if !defined(TESTSRC)
.error "Please define TESTSRC to the absolute path of the test sources, e.g. contrib/netbsd-tests/lib/libc/stdio"
.endif

.PATH: ${TESTSRC}

LIBNETBSD_SRCDIR=	${SRCTOP}/lib/libnetbsd
LIBNETBSD_OBJDIR=	${OBJTOP}/lib/libnetbsd

.for t in ${NETBSD_ATF_TESTS_C}
CFLAGS.$t+=	-I${LIBNETBSD_SRCDIR} -I${SRCTOP}/contrib/netbsd-tests
LDFLAGS.$t+=	-L${LIBNETBSD_OBJDIR}

DPADD.$t+=	${LIBNETBSD}
LDADD.$t+=	-lnetbsd

SRCS.$t?=	${t:C/^/t_/:C/_test$//g}.c
.endfor

ATF_TESTS_C+=	${NETBSD_ATF_TESTS_C}

# A C++ analog isn't provided because there aren't any C++ testcases in
# contrib/netbsd-tests

.for t in ${NETBSD_ATF_TESTS_SH}
ATF_TESTS_SH_SRC_$t?=	${t:C/^/t_/:C/_test$//g}.sh
.endfor

ATF_TESTS_SH+=	${NETBSD_ATF_TESTS_SH}

.endif

# vim: syntax=make
