
PROG=	decryptcore

LIBADD=	crypto pjdlog
CFLAGS+=-DOPENSSL_API_COMPAT=0x10100000L

MAN=	decryptcore.8

CFLAGS+=-I${SRCTOP}/lib/libpjdlog

.include <bsd.prog.mk>
