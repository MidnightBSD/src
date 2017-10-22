# $FreeBSD: release/7.0.0/tools/regression/lib/libc/gen/Makefile 110405 2003-02-05 16:40:18Z mike $

TESTS=	test-fpclassify test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
