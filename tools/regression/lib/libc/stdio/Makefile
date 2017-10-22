# $FreeBSD: release/7.0.0/tools/regression/lib/libc/stdio/Makefile 142842 2005-03-01 01:43:05Z das $

TESTS=	test-perror test-printfloat test-scanfloat
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
