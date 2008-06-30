# $FreeBSD: src/bin/ed/Makefile,v 1.31 2004/12/21 10:15:59 ru Exp $
# $MidnightBSD: src/bin/ed/Makefile,v 1.2 2006/07/07 15:55:59 laffer1 Exp $

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
WARNS?=	2
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if !defined(RELEASE_CRUNCH)
.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
CFLAGS+=-DDES
DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto
.endif
.endif

.include <bsd.prog.mk>
