.include <src.opts.mk>

LIB=	mj
SHLIB_MAJOR=	2

NO_LINT=

SRCS=	mj.c

INCS=	netpgp.h
INCSDIR=${INCLUDEDIR}

NETPGP_SRC=	${SRCTOP}/crypto/netpgp

CFLAGS+= -I${.CURDIR} -I${NETPGP_SRC}/include -I${NETPGP_SRC}/src/libmj
MAN= libmj.3
WARNS=4

LIBADD+=	bz2 crypto z 

.include <bsd.lib.mk>

.PATH: ${NETPGP_SRC}/include ${NETPGP_SRC}/src/libmj
