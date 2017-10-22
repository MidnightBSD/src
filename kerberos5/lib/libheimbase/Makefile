#$FreeBSD: release/10.0.0/kerberos5/lib/libheimbase/Makefile 236337 2012-05-30 22:21:25Z obrien $

LIB=	heimbase
LDFLAGS=	-Wl,--no-undefined
LDADD=	-lpthread
VERSION_MAP= ${KRB5DIR}/base/version-script.map

INCS=	heimbase.h

SRCS= \
	array.c \
	bool.c \
	dict.c \
	heimbase.c \
	null.c \
	number.c \
	string.c

CFLAGS+=	-I${KRB5DIR}/include

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/base
