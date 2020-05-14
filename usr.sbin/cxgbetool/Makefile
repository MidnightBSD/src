# $FreeBSD: stable/11/usr.sbin/cxgbetool/Makefile 346805 2019-04-28 06:51:59Z np $

PROG=	cxgbetool
MAN=	cxgbetool.8
SRCS=	cxgbetool.c
SRCS+=	tcb_common.c
SRCS+=	tcbinfot4.c tcbshowt4.c
SRCS+=	tcbinfot5.c tcbshowt5.c
SRCS+=	tcbinfot6.c tcbshowt6.c
CFLAGS+= -I${SRCTOP}/sys/dev/cxgbe -I${SRCTOP}/sys -I.
LIBADD=	pcap
WARNS?= 2

.include <bsd.prog.mk>
