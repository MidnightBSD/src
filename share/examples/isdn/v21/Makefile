# $FreeBSD: release/7.0.0/share/examples/isdn/v21/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG=		v21modem
NO_MAN=
DPADD=		${LIBUTIL} ${LIBM}
LDADD=		-lutil -lm
CFLAGS+=	-g -Wall

.if !defined(BSDTYPE)
BSDTYPE!=uname
.endif

.include <bsd.prog.mk>
