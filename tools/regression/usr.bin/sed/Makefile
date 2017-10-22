# $FreeBSD: release/7.0.0/tools/regression/usr.bin/sed/Makefile 167569 2007-03-14 18:58:59Z dds $

all:
	@m4 ${.CURDIR}/../regress.m4 ${.CURDIR}/regress.sh | sh /dev/stdin ${.CURDIR}
	@sh multitest.t
