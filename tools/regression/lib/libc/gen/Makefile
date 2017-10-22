# $FreeBSD: stable/9/tools/regression/lib/libc/gen/Makefile 223907 2011-07-10 14:20:11Z jilles $

TESTS=	test-fmtcheck test-fnmatch test-fpclassify test-posix_spawn test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}

sh-tests: test-fnmatch
	./test-fnmatch -s 1 >../../../bin/sh/builtins/case2.0
	./test-fnmatch -s 2 >../../../bin/sh/builtins/case3.0
