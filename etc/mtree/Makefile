# $MidnightBSD: src/etc/mtree/Makefile,v 1.3 2012/03/10 03:35:45 laffer1 Exp $
# $FreeBSD: src/etc/mtree/Makefile,v 1.5 2004/12/21 08:46:50 ru Exp $

.include <bsd.own.mk>

FILES=	${_BIND.chroot.dist} \
	${_BIND.include.dist} \
	BSD.include.dist \
	BSD.local.dist \
	BSD.root.dist \
	${_BSD.sendmail.dist} \
	BSD.usr.dist \
	BSD.var.dist \
	BSD.x11-4.dist \
	BSD.x11.dist

.if ${MK_BIND} != "no"
_BIND.chroot.dist=	BIND.chroot.dist
.if ${MK_BIND_LIBS} != "no"
_BIND.include.dist=	BIND.include.dist
.endif
.endif
.if ${MK_GROFF} != "no"
_BSD.groff.dist=	BSD.groff.dist
.endif
.if ${MK_SENDMAIL} != "no"
_BSD.sendmail.dist=	BSD.sendmail.dist
.endif

NO_OBJ=
FILESDIR= /etc/mtree

.include <bsd.prog.mk>
