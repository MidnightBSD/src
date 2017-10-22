# $FreeBSD: stable/9/kerberos5/tools/make-roken/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG=	make-roken
NO_MAN=

CLEANFILES= make-roken.c

.include <bsd.prog.mk>

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}
