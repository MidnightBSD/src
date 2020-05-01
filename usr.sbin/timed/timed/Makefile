#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.sbin/timed/timed/Makefile 275054 2014-11-25 16:57:27Z bapt $
#
# optional flags are: MEASURE TESTING DEBUG

PROG=	timed
MAN=	timed.8
SRCS=	acksend.c candidate.c correct.c master.c networkdelta.c readmsg.c \
	slave.c timed.c byteorder.c measure.c cksum.c

LIBADD=	util

WARNS?=	1

.include "../../Makefile.inc"
.include <bsd.prog.mk>
