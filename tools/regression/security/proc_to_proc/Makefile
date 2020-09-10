# $FreeBSD: stable/11/tools/regression/security/proc_to_proc/Makefile 264400 2014-04-13 05:21:56Z imp $

PROG=	testuid
MAN=

ADDITIONAL_FLAGS=-DSETSUGID_SUPPORTED
#ADDITIONAL_FLAGS=	-DSETSUGID_SUPPORTED_BUT_NO_LIBC_STUB
ADDITIONAL_FLAGS+=	-DCHECK_CRED_SET
CFLAGS+=	${ADDITIONAL_FLAGS}
SRCS=	testuid.c scenario.c

.include <bsd.prog.mk>
