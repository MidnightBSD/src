# $FreeBSD: release/10.0.0/tools/regression/lib/libmp/Makefile 160786 2006-07-28 16:00:59Z simon $

TESTS=	test-libmp
CFLAGS+= -g -Wall -lcrypto -lmp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
