# $FreeBSD: release/10.0.0/usr.sbin/pw/Makefile 242349 2012-10-30 08:00:53Z bapt $

PROG=	pw
MAN=	pw.conf.5 pw.8
SRCS=	pw.c pw_conf.c pw_user.c pw_group.c pw_log.c pw_nis.c pw_vpw.c \
	grupd.c pwupd.c fileupd.c psdate.c \
	bitmap.c cpdir.c rm_r.c

WARNS?=	2

DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.include <bsd.prog.mk>
