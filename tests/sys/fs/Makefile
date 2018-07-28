# $MidnightBSD$
# $FreeBSD: stable/10/tests/sys/fs/Makefile 313519 2017-02-10 03:17:11Z ngie $

TESTSDIR=		${TESTSBASE}/sys/fs

TESTSRC=		${SRCTOP}/contrib/netbsd-tests/fs

#TESTS_SUBDIRS+=	nullfs	# XXX: needs rump
TESTS_SUBDIRS+=		tmpfs

FILES+=			h_funcs.subr
FILESDIR=		${TESTSDIR}

CLEANFILES+=		h_funcs.subr
CLEANFILES+=		h_funcs.subr.tmp

h_funcs.subr: ${TESTSRC}/h_funcs.subr
	cat ${.ALLSRC} | \
	    sed -e '/atf_require_prog mount_$${name}/d' >>${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}

.include <bsd.test.mk>
