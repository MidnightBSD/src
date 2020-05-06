# $FreeBSD: stable/11/libexec/rtld-elf/tests/target/Makefile 316388 2017-04-02 02:16:01Z asomers $

.include <bsd.own.mk>

PROG=		target
BINDIR=		${TESTSBASE}/libexec/rtld-elf

WARNS?=		3
CFLAGS+=	-I${.CURDIR}/../libpythagoras

LDFLAGS+=	-L${.OBJDIR}/../libpythagoras
DPADD+=		${.OBJDIR}/../libpythagoras/libpythagoras.a
LDADD=		-lpythagoras

MAN=

.include <bsd.prog.mk>
