# $FreeBSD: release/10.0.0/tools/regression/lib/libc/gen/Makefile 250825 2013-05-20 13:05:51Z jilles $

TESTS=	test-arc4random test-fmtcheck test-fmtmsg test-fnmatch \
	test-fpclassify test-ftw test-popen test-posix_spawn test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}

sh-tests: test-fnmatch
	./test-fnmatch -s 1 >../../../bin/sh/builtins/case2.0
	./test-fnmatch -s 2 >../../../bin/sh/builtins/case3.0
