# $FreeBSD: stable/11/usr.sbin/keyserv/Makefile 275054 2014-11-25 16:57:27Z bapt $

PROG=	keyserv
MAN=	keyserv.8
SRCS=	keyserv.c setkey.c crypt_svc.c crypt_server.c crypt.h

CFLAGS+= -DKEYSERV_RANDOM -DBROKEN_DES -I.

LIBADD=	mp rpcsvc

WARNS?=	1

RPCDIR=	${DESTDIR}/usr/include/rpcsvc

CLEANFILES= crypt_svc.c crypt.h

RPCGEN= RPCGEN_CPP=${CPP:Q} rpcgen -C

crypt_svc.c: ${RPCDIR}/crypt.x
	${RPCGEN} -m -o ${.TARGET} ${RPCDIR}/crypt.x

crypt.h: ${RPCDIR}/crypt.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/crypt.x

.include <bsd.prog.mk>
