# $FreeBSD: release/10.0.0/tools/regression/usr.bin/cc/Makefile 230368 2012-01-20 06:57:21Z das $

TESTS=	float
CFLAGS+=-lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
