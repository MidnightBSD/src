.include <src.opts.mk>

LIB=	netpgp
SHLIB_MAJOR=	2

NO_LINT=

SRCS=	bufgap.c compress.c create.c crypto.c \
	keyring.c misc.c netpgp.c openssl_crypto.c packet-parse.c \
	packet-print.c packet-show.c reader.c signature.c \
	ssh2pgp.c symmetric.c validate.c writer.c

INCS=	netpgp.h
INCSDIR=${INCLUDEDIR}

NETPGP_SRC=	${SRCTOP}/crypto/netpgp

CFLAGS+= -I${.CURDIR} -I${NETPGP_SRC}/include -I${NETPGP_SRC}/src/libmj
MAN= libnetpgp.3
WARNS=0

LIBADD+=	bz2 crypto mj z 

.include <bsd.lib.mk>

.PATH: ${NETPGP_SRC}/include ${NETPGP_SRC}/src/lib
