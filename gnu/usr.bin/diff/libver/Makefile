.include 	"../Makefile.inc"

LIB=		ver
INTERNALLIB=	true

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../libdiffutils
CFLAGS+=	-DHAVE_CONFIG_H

WARNS=	0

SRCS=		version.c

.include <bsd.lib.mk>
