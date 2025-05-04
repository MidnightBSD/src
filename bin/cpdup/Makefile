PROG=	cpdup
SRCS=	cpdup.c hcproto.c hclink.c misc.c

.if defined(BOOTSTRAPPING) || defined(RESCUE)
# For boostrapping buildworld the md5 functionality is not needed
CFLAGS+=	-DNOMD5
.else
.if !defined(NOMD5)
SRCS+=		md5.c
LIBADD+=	crypto md
.endif
.endif

WARNS?=	1

.include <bsd.prog.mk>

