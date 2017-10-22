# $FreeBSD: release/10.0.0/tools/regression/lib/libutil/Makefile 256130 2013-10-07 22:22:57Z jmg $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen test-grp \
	test-pidfile test-humanize_number
CFLAGS+= -g -Wall -Wextra -Werror -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
