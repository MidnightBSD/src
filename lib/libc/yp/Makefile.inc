#	from: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
# $FreeBSD: release/7.0.0/lib/libc/yp/Makefile.inc 156613 2006-03-13 01:15:01Z deischen $

# yp sources
.PATH: ${.CURDIR}/yp

SRCS+=	xdryp.c yp.h yp_xdr.c yplib.c
CLEANFILES+=	yp.h yp_xdr.c

SYM_MAPS+=	${.CURDIR}/yp/Symbol.map

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/yp.x
RPCGEN=	rpcgen -C

yp_xdr.c: ${RPCSRC}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}
