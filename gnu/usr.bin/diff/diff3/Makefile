.include "../Makefile.inc0"
.PATH:		${SRCDIR}/src ${SRCDIR}/man

PROG=		diff3
CFLAGS+=	-I${SRCDIR}/lib -I${.CURDIR}/../libdiffutils
CFLAGS+=	-I${.CURDIR}/../diff -I${.CURDIR}/../libver

#.if ${MK_ICONV} != "no"
#DPADD+=  ${LIBICONV}
#LDADD+=  -liconv
#.endif

.include <bsd.prog.mk>
