# $FreeBSD: release/10.0.0/usr.sbin/yppush/Makefile 231118 2012-02-07 09:27:07Z dim $

RPCDIR=	${.CURDIR}/../../include/rpcsvc
.PATH: ${RPCDIR} ${.CURDIR}/../../usr.sbin/ypserv \
       ${.CURDIR}/../../libexec/ypxfr

PROG=	yppush
MAN=	yppush.8
SRCS=	ypxfr_getmap.c yp_dblookup.c yp_error.c ypxfr_misc.c yppush_main.c \
	${GENSRCS}
GENSRCS=yp.h yp_clnt.c yppush_svc.c

CFLAGS+= -I. -I${.CURDIR}/../../libexec/ypxfr

WARNS?=	2

RPCGEN= RPCGEN_CPP=${CPP:Q} rpcgen -C

CLEANFILES= ${GENSRCS}

yppush_svc.c: yp.x
	${RPCGEN} -DYPPUSH_ONLY -m -o ${.TARGET} ${RPCDIR}/yp.x

yp_clnt.c: yp.x
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp.h: yp.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yp.x

.include <bsd.prog.mk>
