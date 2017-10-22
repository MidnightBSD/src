#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: release/10.0.0/share/doc/usd/11.vitut/Makefile 136910 2004-10-24 15:33:08Z ru $

VOLUME=		usd/11.edit
SRCS=		edittut.ms
MACROS=		-ms
USE_TBL=
SRCDIR=		${.CURDIR}/../../../../contrib/nvi/docs/USD.doc/edit

# index for versatec is different from the one in edit.tut
# because the fonts are different and entries reference page
# rather than section numbers.  if you have a typesetter
# you should just use the index in edit.tut, and ignore editvindex.

#editvindex:
#	${TROFF} ${MACROS} -n22 edit.vindex

.include <bsd.doc.mk>
