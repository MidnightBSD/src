# $FreeBSD: stable/11/libexec/rtld-elf/tests/Makefile 316388 2017-04-02 02:16:01Z asomers $

SUBDIR+=	libpythagoras target

SUBDIR_DEPEND_target=	libpythagoras
ATF_TESTS_C=	ld_library_pathfds
WARNS?=		3

.include <bsd.test.mk>
