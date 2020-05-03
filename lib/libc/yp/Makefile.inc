#	from: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
# $FreeBSD: stable/11/lib/libc/yp/Makefile.inc 302255 2016-06-28 19:53:16Z bdrewery $

# yp sources
.PATH: ${LIBC_SRCTOP}/yp

SRCS+=	xdryp.c yp.h yp_xdr.c yplib.c
CLEANFILES+=	yp.h yp_xdr.c

SYM_MAPS+=	${LIBC_SRCTOP}/yp/Symbol.map

RPCSRC=	${SRCTOP}/include/rpcsvc/yp.x
RPCGEN=	RPCGEN_CPP=${CPP:Q} rpcgen -C

yp_xdr.c: ${RPCSRC}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}
