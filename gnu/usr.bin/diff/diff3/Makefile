.include "../Makefile.inc0"
.PATH:		${SRCDIR}/src ${SRCDIR}/man

PROG=		diff3
CFLAGS+=	-I${SRCDIR}/lib -I${.CURDIR}/../libdiffutils
CFLAGS+=	-I${.CURDIR}/../diff -I${.CURDIR}/../libver

DPADD+=  ${LIBICONV}
LDADD+=  -liconv

.include <bsd.prog.mk>
