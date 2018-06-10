# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/extattr/Makefile 301145 2016-06-01 17:22:15Z asomers $

.include <bsd.own.mk>

PROG=	rmextattr
MAN=	rmextattr.8

LDADD=	-lsbuf

LINKS+=	${BINDIR}/rmextattr ${BINDIR}/getextattr
LINKS+=	${BINDIR}/rmextattr ${BINDIR}/setextattr
LINKS+=	${BINDIR}/rmextattr ${BINDIR}/lsextattr

MLINKS+=	rmextattr.8 setextattr.8
MLINKS+=	rmextattr.8 getextattr.8
MLINKS+=	rmextattr.8 lsextattr.8

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
