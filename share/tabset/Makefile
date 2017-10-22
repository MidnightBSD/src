#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: release/10.0.0/share/tabset/Makefile 88055 2001-12-17 13:59:35Z ru $

FILES=	3101 9837 aa aed512 beehive diablo dtc382 hp700-wy ibm3101 std \
	stdcrt tandem653 teleray vt100 vt100-w wyse-adds xerox1720 xerox1730 \
	xerox1730-lm zenith29

CLEANFILES+=	${FILES}

.SUFFIXES: .uu
.uu:
	uudecode < ${.IMPSRC}

FILESDIR=	${BINDIR}/tabset

.include <bsd.prog.mk>
