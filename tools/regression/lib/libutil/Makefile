# $FreeBSD: release/7.0.0/tools/regression/lib/libutil/Makefile 171707 2007-08-03 11:29:49Z des $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
