# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/makewhatis/Makefile 291819 2015-12-04 19:16:56Z bdrewery $

PROG=		makewhatis
DPADD=		${LIBZ}
LDADD=		-lz
SCRIPTS=	makewhatis.local.sh
MAN=		makewhatis.1 makewhatis.local.8
SCRIPTSDIR=	${LIBEXECDIR}
LINKS=		${SCRIPTSDIR}/makewhatis.local ${SCRIPTSDIR}/catman.local
MLINKS=		makewhatis.local.8 catman.local.8

.include <bsd.prog.mk>
