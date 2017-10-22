# $FreeBSD: release/10.0.0/tools/regression/lib/libc/stdio/Makefile 254151 2013-08-09 17:24:23Z jilles $

TESTS=	test-fmemopen test-getdelim test-mkostemp test-open_memstream \
	test-open_wmemstream test-perror test-print-positional test-printbasic \
	test-printfloat test-scanfloat 
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
