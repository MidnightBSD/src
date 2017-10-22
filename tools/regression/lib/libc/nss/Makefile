# $FreeBSD: release/10.0.0/tools/regression/lib/libc/nss/Makefile 168754 2007-04-15 11:02:31Z bushman $

TESTS=	test-getaddr test-getgr test-gethostby test-getpw test-getproto\
	test-getrpc test-getserv test-getusershell
CFLAGS+= -g -Wall

.PHONY: tests
tests: ${TESTS}

.PHONY: clean
clean:
	-rm -f ${TESTS}
