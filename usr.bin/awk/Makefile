# $FreeBSD: stable/11/usr.bin/awk/Makefile 314012 2017-02-21 02:33:28Z ngie $

AWKSRC=	${SRCTOP}/contrib/one-true-awk
.PATH: ${AWKSRC}

PROG=	awk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c ytab.h

CFLAGS+= -DHAS_ISBLANK -I. -I${AWKSRC} -DFOPEN_MAX=64

WARNS?=	1

LIBADD=	m

LINKS=	${BINDIR}/awk ${BINDIR}/nawk
MLINKS=	awk.1 nawk.1

CLEANFILES= maketab proctab.c ytab.h

# XXX: awkgram.c isn't really needed here but it is added to keep
# awkgram.h: .NOMETA respected.
ytab.h: awkgram.c awkgram.h .NOMETA
	ln -sf ${.ALLSRC:M*.h} ${.TARGET}

proctab.c: maketab
	${BTOOLSPATH:U.}/maketab > proctab.c

build-tools: maketab
maketab: ytab.h maketab.c ${BUILD_TOOLS_META}

.include <bsd.prog.mk>
