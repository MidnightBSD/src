# $MidnightBSD$
# $FreeBSD: stable/10/tests/Makefile 291792 2015-12-04 18:14:31Z bdrewery $

.include <bsd.own.mk>

SUBDIR+= etc
SUBDIR+= sys
SUBDIR_PARALLEL=

TESTSDIR= ${TESTSBASE}
KYUAFILE= yes

afterinstall: install-tests-local
install-tests-local: .PHONY
	${INSTALL_SYMLINK} ../local/tests ${DESTDIR}${TESTSDIR}/local

.include <bsd.test.mk>
