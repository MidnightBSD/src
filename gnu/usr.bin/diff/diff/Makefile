.include <bsd.own.mk>

.include "../Makefile.inc0"
.PATH:	${SRCDIR}/src ${SRCDIR}/man

PROG=		diff
CFLAGS+=	-I${SRCDIR}/lib -I${.CURDIR}/../libdiffutils
CFLAGS+=	-I${.CURDIR}/../libver

SRCS=		analyze.c \
		context.c \
		diff.c \
		dir.c \
		ed.c \
		ifdef.c \
		io.c \
		normal.c \
		side.c \
		util.c

#.if ${MK_ICONV} != "no"
#DPADD+=  ${LIBICONV}
#LDADD+=  -liconv
#.endif

.include <bsd.prog.mk>
