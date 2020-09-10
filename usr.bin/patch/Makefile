#	$OpenBSD: Makefile,v 1.4 2005/05/16 15:22:46 espie Exp $
# $FreeBSD: stable/11/usr.bin/patch/Makefile 355351 2019-12-03 18:55:09Z kevans $

.include <src.opts.mk>

PROG=	patch

SRCS=	backupfile.c inp.c mkpath.c patch.c pch.c util.c

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
