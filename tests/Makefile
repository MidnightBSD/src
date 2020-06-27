# $FreeBSD: stable/11/tests/Makefile 299094 2016-05-04 23:20:53Z ngie $

PACKAGE= tests

TESTSDIR= ${TESTSBASE}

${PACKAGE}FILES+=		README

KYUAFILE= yes

SUBDIR+= etc
SUBDIR+= sys

SUBDIR_PARALLEL=

afterinstall: install-tests-local
install-tests-local: .PHONY
	${INSTALL_SYMLINK} ../local/tests ${DESTDIR}${TESTSDIR}/local

.include <bsd.test.mk>
