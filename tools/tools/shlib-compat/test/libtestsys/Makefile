# $FreeBSD: release/10.0.0/tools/tools/shlib-compat/test/libtestsys/Makefile 248693 2013-03-25 00:31:14Z gleb $

TESTNUM=	sys

CLEANFILES+= test.c

.include <../Makefile.inc>
.include <bsd.lib.mk>

test.c: ../../makesyscalls-fake.sh
	sh ${.CURDIR}/../../makesyscalls-fake.sh /usr/src/sys/kern/syscalls.master > ${.TARGET}
