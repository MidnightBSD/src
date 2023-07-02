# $DragonFly: src/bin/cpdup/Makefile,v 1.7 2008/04/10 22:09:08 dillon Exp $

PROG=	cpdup
SRCS=	cpdup.c hcproto.c hclink.c misc.c fsmid.c

.if !defined(NOPTHREADS) && !defined(BOOTSTRAPPING)
CFLAGS += -DUSE_PTHREADS=1
LIBADD+=	pthread
.endif

.if !defined(NOMD5)
SRCS+=	md5.c
.endif

WARNS?= 1

LIBADD+=	md

.include <bsd.prog.mk>

