# $FreeBSD: stable/9/lib/libsmb/Makefile 195767 2009-07-19 17:25:24Z kensmith $

CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
.PATH: ${CONTRIBDIR}/lib/smb

LIB=	smb
SHLIB_MAJOR=	4
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

SRCS=	rcfile.c ctx.c cfopt.c subr.c nls.c rap.c mbuf.c rq.c file.c \
	print.c \
	kiconv.c \
	nb.c nb_name.c nb_net.c nbns_rq.c

WARNS?=	1
CFLAGS+= -DSMB_CFG_FILE=\"/etc/nsmb.conf\" -I${CONTRIBDIR}/include

.include <bsd.lib.mk>
