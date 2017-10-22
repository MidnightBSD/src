# $FreeBSD: stable/9/bin/ed/Makefile 204711 2010-03-04 16:08:01Z uqs $

.include <bsd.own.mk>

PROG=	ed
SRCS=	buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if !defined(RELEASE_CRUNCH)
.if ${MK_OPENSSL} != "no"
CFLAGS+=-DDES
DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto
.endif
.endif

.include <bsd.prog.mk>
