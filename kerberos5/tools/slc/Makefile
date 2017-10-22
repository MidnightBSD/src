# $FreeBSD: release/10.0.0/kerberos5/tools/slc/Makefile 241298 2012-10-06 20:01:05Z marcel $

PROG=	slc
LIBROKEN_A=     ${.OBJDIR}/../../lib/libroken/libroken.a
LDADD=  ${LIBROKEN_A} ${LIBVERS}
DPADD=  ${LIBROKEN_A} ${LIBVERS}
NO_MAN=

SRCS=	roken.h \
	slc-gram.y \
	slc-lex.l

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl -I${KRB5DIR}/lib/vers -I.

CLEANFILES= roken.h slc-gram.c slc-lex.c

roken.h:
	${MAKE_ROKEN} > ${.TARGET}

#	${.OBJDIR}/../make-roken/make-roken > ${.TARGET}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/roken ${KRB5DIR}/lib/sl
