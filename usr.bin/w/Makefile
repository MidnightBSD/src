#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/w/Makefile 319186 2017-05-30 04:27:05Z ngie $

PROG=	w
SRCS=	fmt.c pr_time.c proc_compare.c w.c
MAN=	w.1 uptime.1
LIBADD=	kvm sbuf xo util
LINKS=	${BINDIR}/w ${BINDIR}/uptime

.PATH: ${SRCTOP}/bin/ps

.include <bsd.prog.mk>
