# $MidnightBSD$
#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD: stable/10/usr.sbin/bootparamd/callbootd/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	callbootd
MAN=
SRCS=	callbootd.c ${GENSRCS}
GENSRCS=bootparam_prot.h bootparam_prot_clnt.c bootparam_prot_xdr.c

CFLAGS+= -I.

CLEANFILES= ${GENSRCS}

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x

bootparam_prot_clnt.c: ${RPCSRC}
	RPCGEN_CPP=${CPP:Q} rpcgen -C -l -o ${.TARGET} ${RPCSRC}

bootparam_prot_xdr.c: ${RPCSRC}
	RPCGEN_CPP=${CPP:Q} rpcgen -C -c -o ${.TARGET} ${RPCSRC}

bootparam_prot.h: ${RPCSRC}
	RPCGEN_CPP=${CPP:Q} rpcgen -C -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
