# $FreeBSD: release/10.0.0/tools/regression/lib/libc/regex/Makefile 132392 2004-07-19 09:00:26Z tjr $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
