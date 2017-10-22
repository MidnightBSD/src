# $FreeBSD: stable/9/usr.bin/csup/Makefile 230743 2012-01-29 14:55:20Z marius $

PROG=	csup
SRCS=	attrstack.c \
	auth.c \
	config.c \
	detailer.c \
	diff.c \
	fattr.c \
	fixups.c \
	fnmatch.c \
	globtree.c \
	idcache.c \
	keyword.c \
	lex.rcs.c \
	lister.c \
	main.c \
	misc.c \
	mux.c \
	parse.y \
	pathcomp.c \
	proto.c \
	rcsfile.c \
	rcsparse.c \
	rsyncfile.c \
	status.c \
	stream.c \
	threads.c \
	token.l \
	updater.c

CFLAGS+= -I. -I${.CURDIR}
CFLAGS+= -DHAVE_FFLAGS -DNDEBUG
WARNS?=	1

DPADD=	${LIBMD} ${LIBZ} ${LIBPTHREAD}
LDADD=	-lmd -lz -lpthread

SCRIPTS=	cpasswd.sh
MAN=		csup.1 cpasswd.1

.include <bsd.prog.mk>
