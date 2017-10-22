# $FreeBSD: release/10.0.0/usr.bin/csup/Makefile 228857 2011-12-24 12:16:38Z marius $

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
