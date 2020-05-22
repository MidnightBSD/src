# $FreeBSD: stable/11/share/vt/fonts/Makefile 338917 2018-09-24 18:41:32Z emaste $

FONTS=	gallant.fnt \
	terminus-b32.fnt \
	vgarom-8x8.fnt \
	vgarom-8x14.fnt \
	vgarom-8x16.fnt \
	vgarom-16x32.fnt \
	vgarom-thin-8x8.fnt \
	vgarom-thin-8x16.fnt
FILES=	${FONTS} INDEX.fonts

CLEANFILES+=	${FONTS}

.SUFFIXES: .fnt .fnt.uu .hex
.hex.fnt:
	vtfontcvt ${.IMPSRC} ${.TARGET}

FILESDIR=	${SHAREDIR}/vt/fonts

.include <bsd.prog.mk>
