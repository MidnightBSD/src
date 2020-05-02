# $FreeBSD: stable/11/kerberos5/tools/make-roken/Makefile 264400 2014-04-13 05:21:56Z imp $

PROG=	make-roken
MAN=

CLEANFILES= make-roken.c

.include "../Makefile.inc"

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
