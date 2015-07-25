# $FreeBSD: stable/10/kerberos5/tools/make-roken/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	make-roken
MAN=

CLEANFILES= make-roken.c

.include "../Makefile.inc"

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
