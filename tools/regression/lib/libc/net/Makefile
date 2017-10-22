# $FreeBSD: release/10.0.0/tools/regression/lib/libc/net/Makefile 169523 2007-05-13 14:03:21Z rwatson $

TESTS=	test-ether test-eui64_aton test-eui64_line test-eui64_ntoa
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
