#	$MidnightBSD: src/bin/ksh/Makefile,v 1.4 2006/08/18 23:13:16 adam Exp $
#	$OpenBSD: Makefile,v 1.25 2005/10/06 06:39:35 otto Exp $

PROG=	ksh
INSTALLFLAGS= -S
SRCS=	alloc.c c_ksh.c c_sh.c c_test.c c_ulimit.c edit.c emacs.c eval.c \
	exec.c expr.c history.c io.c jobs.c lex.c mail.c main.c mknod.c \
	misc.c path.c shf.c syn.c table.c trap.c tree.c tty.c var.c \
	version.c vi.c

DEFS=	-Wall -ansi -pedantic -std=c99
CFLAGS+=${DEFS} -I. -I${.CURDIR}
LDFLAGS+=-static
WARNS?=	3
NO_WERROR=yes
MAN=	ksh.1 

CLEANFILES+=	emacs.out

LINKS=	${BINDIR}/ksh ${BINDIR}/rksh
MLINKS=	ksh.1 rksh.1 ksh.1 ulimit.1

.depend emacs.o: emacs.out

emacs.out: emacs.c
	/bin/sh ${.CURDIR}/emacs-gen.sh ${.CURDIR}/emacs.c > emacs.out

check test:
	/usr/bin/perl ${.CURDIR}/tests/th -s ${.CURDIR}/tests -p ./ksh -C \
	    pdksh,sh,ksh,posix,posix-upu

.include <bsd.prog.mk>

