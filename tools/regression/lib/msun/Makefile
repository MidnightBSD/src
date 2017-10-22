# $FreeBSD: release/7.0.0/tools/regression/lib/msun/Makefile 161526 2006-08-22 08:03:01Z ru $

TESTS=	test-fenv test-ilogb test-lrint test-lround test-next test-rem
CFLAGS+= -O0 -lm
ARCH!=	uname -m

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
