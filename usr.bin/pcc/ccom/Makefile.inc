# $MidnightBSD: src/usr.bin/pcc/ccom/Makefile.inc,v 1.4 2008/05/15 05:55:30 laffer1 Exp $
#	$OpenBSD: Makefile.inc,v 1.3 2007/10/15 01:05:25 deraadt Exp $
#
# Makefile for the ccom part of pcc.
#

PROG=		ccom
#PROG=  	ccom_${TARGMACH}
MAN=		ccom.1
MANDIR=		/usr/local/man/man
BINDIR=		${libexecdir}
TARGOS=		midnightbsd
CLEANFILES=	external.h external.c mkext

CFLAGS+=	-DPCC_DEBUG -DGCC_COMPAT
CFLAGS+=	-Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${libexecdir}\"
CFLAGS+=	-I. -I${.CURDIR}/.. -I${.CURDIR} -I${.CURDIR}/../../mip
CFLAGS+=	-I${.CURDIR}/../../${TARGMACH}
CFLAGS+=	-I${.CURDIR}/../..

.PATH:	${.CURDIR}/../../${TARGMACH}
.PATH:	${.CURDIR}/../../mip
.PATH:	${.CURDIR}/..

SRCS=optim.c pftn.c scan.l pass2.h trees.c cgram.y inline.c symtabs.c \
	gcc_compat.c init.c local.c code.c stabs.c match.c reader.c optim2.c \
	regs.c local2.c order.c table.c common.c main.c external.c

cgram.c: cgram.y
	$(YACC) $(YFLAGS) -d $<
	mv y.tab.c cgram.c
	mv y.tab.h cgram.h

pass2.h: external.h

optim2.o reader.o: external.h

external.h external.c: mkext.c table.c common.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMKEXT -o mkext \
	    ${.CURDIR}/../../mip/mkext.c ${.CURDIR}/../../${TARGMACH}/table.c \
	    ${.CURDIR}/../../mip/common.c
	./mkext

pftn.o: cgram.c

cleandepend:
	rm -f .depend ${.CURDIR}/tags cgram.h

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
