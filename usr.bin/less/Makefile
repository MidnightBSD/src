# $FreeBSD: src/usr.bin/less/Makefile,v 1.10 2005/05/17 11:41:01 ru Exp $
# $MidnightBSD: src/usr.bin/less/Makefile,v 1.3 2011/10/20 14:28:04 laffer1 Exp $

PROG=	less
SRCS=	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c cvt.c decode.c \
	edit.c filename.c forwback.c help.c ifile.c input.c jump.c line.c \
	linenum.c lsystem.c mark.c optfunc.c option.c opttbl.c os.c output.c \
	pattern.c position.c prompt.c search.c signal.c tags.c ttyin.c version.c
SCRIPTS=lesspipe.sh zless.sh
SCRIPTSNAME_lesspipe.sh=lesspipe.sh
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
LINKS=	${BINDIR}/less ${BINDIR}/more \
	${BINDIR}/zless ${BINDIR}/bzless \
	${BINDIR}/zless ${BINDIR}/xzless \
	${BINDIR}/zless ${BINDIR}/lzless
MLINKS=	less.1 more.1
CLEANFILES= less.1

WARNS?=	2

.include "Makefile.common"
.include <bsd.prog.mk>
