# $MidnightBSD$

CPIODIR= ${.CURDIR}/../../../contrib/cpio
.PATH: ${CPIODIR}/lib ${CPIODIR}/src ${CPIODIR}/doc

SUBDIR=	doc

PROG=	cpio
SRCS=   copyin.c \
	copyout.c \
	copypass.c \
	defer.c \
	dstring.c \
	filemode.c \
	global.c \
	idcache.c \
	main.c \
	makepath.c \
	tar.c \
	userspec.c \
	util.c \
	argp-ba.c \
	argp-eexst.c \
	argp-fmtstream.c \
	argp-fs-xinl.c \
	argp-help.c \
	argp-parse.c \
	argp-pin.c \
	argp-pv.c \
	argp-pvh.c \
	argp-xinl.c \
	basename.c \
	dirname.c \
	dup-safer.c \
	error.c \
	exitfail.c \
	fatal.c \
	fd-safer.c \
	full-write.c \
	getopt.c \
	getopt1.c \
	hash.c \
	mbchar.c \
	mempcpy.c \
	open-safer.c \
	paxconvert.c \
	paxerror.c \
	paxexit.c \
	paxnames.c \
	printf-parse.c \
	quote.c \
	quotearg.c \
	rtapelib.c \
	safe-read.c \
	safe-write.c \
	setenv.c \
	stpcpy.c \
	strcasecmp.c \
	strncasecmp.c \
	strchrnul.c \
	stripslash.c \
	strndup.c \
	strnlen.c \
	strnlen1.c \
	umaxtostr.c \
	utimens.c \
	xalloc-die.c \
	xgetcwd.c \
	xmalloc.c \
	xstrndup.c \
	alloca.h \
	getopt.h 

CLEANFILES+= alloca.h getopt.h

getopt.h: getopt_.h
	ln -fs ${.ALLSRC} ${.TARGET}

alloca.h: alloca_.h
	ln -fs ${.ALLSRC} ${.TARGET}

CFLAGS+=-I${.OBJDIR} -I${.CURDIR} -I${CPIODIR}/lib -I${CPIODIR}/src -DHAVE_CONFIG_H \
	-DHAVE_MKFIFO -DHAVE_SETLOCALE -DHAVE_LSTAT

.include <bsd.prog.mk>
