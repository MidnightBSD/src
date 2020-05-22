# $FreeBSD: stable/11/usr.bin/units/Makefile 298257 2016-04-19 07:28:39Z delphij $

.include <src.opts.mk>

PROG=	units
FILES=	definitions.units
FILESDIR=	${SHAREDIR}/misc

LIBADD=	edit

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
