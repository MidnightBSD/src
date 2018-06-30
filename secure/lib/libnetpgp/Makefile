# $MidnightBSD$

.include <bsd.own.mk>

LIB=	netpgp
SHLIB_MAJOR=	1

NO_LINT=

SRCS=	compress.c create.c crypto.c \
	keyring.c misc.c netpgp.c openssl_crypto.c packet-parse.c \
	packet-print.c packet-show.c reader.c signature.c \
	symmetric.c validate.c writer.c

INCS=	netpgp.h
INCSDIR=${INCLUDEDIR}

NETPGP_SRC=	${SRCTOP}/crypto/netpgp

CFLAGS+= -I${.CURDIR} -I${NETPGP_SRC}/include
MAN= libnetpgp.3
WARNS=4

DPADD=	${LIBCRYPTO} ${LIBZ} ${LIBBZ2}
LDADD=	-lcrypto -lz -lbz2

.include <bsd.lib.mk>

.PATH: ${NETPGP_SRC}/include ${NETPGP_SRC}/src/lib
