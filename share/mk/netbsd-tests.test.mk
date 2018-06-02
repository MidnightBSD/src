# $MidnightBSD$
# $FreeBSD: stable/10/share/mk/netbsd-tests.test.mk 292278 2015-12-15 17:58:10Z ngie $

.if !target(__netbsd_tests.test.mk__)
__netbsd_tests.test.mk__:

OBJTOP?=	${.OBJDIR:S/${RELDIR}//}

TESTSRC?=	${SRCTOP}/contrib/netbsd-tests/${RELDIR:H}

.if !exists(${TESTSRC}/)
.error "Please define TESTSRC to the absolute path of the test sources, e.g. $${SRCTOP}/contrib/netbsd-tests/lib/libc/stdio"
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
