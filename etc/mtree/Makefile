# $MidnightBSD$
# $FreeBSD: stable/10/etc/mtree/Makefile 260013 2013-12-28 20:05:31Z jmmv $

.include <bsd.own.mk>

FILES=	${_BSD.debug.dist} \
	BSD.include.dist \
	BSD.local.dist \
	BSD.root.dist \
	${_BSD.sendmail.dist} \
	${_BSD.tests.dist} \
	BSD.usr.dist \
	BSD.var.dist \
	BSD.x11-4.dist \
	BSD.x11.dist

.if ${MK_DEBUG_FILES} != "no"
_BSD.debug.dist=	BSD.debug.dist
.endif
.if ${MK_GROFF} != "no"
_BSD.groff.dist=        BSD.groff.dist
.endif
.if ${MK_SENDMAIL} != "no"
_BSD.sendmail.dist=	BSD.sendmail.dist
.endif
.if ${MK_TESTS} != "no"
_BSD.tests.dist=	BSD.tests.dist
.endif

NO_OBJ=
FILESDIR= /etc/mtree

.include <bsd.prog.mk>
