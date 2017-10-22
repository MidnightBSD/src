# $FreeBSD: stable/9/usr.sbin/bsdinstall/partedit/Makefile 248240 2013-03-13 13:50:50Z nwhitehorn $

BINDIR= /usr/libexec/bsdinstall
PROG=	partedit
LINKS= ${BINDIR}/partedit ${BINDIR}/autopart \
       ${BINDIR}/partedit ${BINDIR}/scriptedpart
LDADD=	-lgeom -lncursesw -lutil -ldialog -lm

PARTEDIT_ARCH= ${MACHINE}
.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
PARTEDIT_ARCH= x86
.endif
.if !exists(partedit_${PARTEDIT_ARCH}.c)
PARTEDIT_ARCH= generic
.endif

SRCS=	diskeditor.c partedit.c gpart_ops.c partedit_${PARTEDIT_ARCH}.c \
	part_wizard.c scripted.c

WARNS?=	3
NO_MAN=	true

.include <bsd.prog.mk>
