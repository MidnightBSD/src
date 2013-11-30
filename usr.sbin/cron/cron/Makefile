# $MidnightBSD: src/usr.sbin/cron/cron/Makefile,v 1.3 2009/10/07 02:32:55 laffer1 Exp $
# $FreeBSD: src/usr.sbin/cron/cron/Makefile,v 1.15 2001/07/20 06:19:40 obrien Exp $

PROG=	cron
MAN=	cron.8
SRCS=	cron.c database.c do_command.c job.c user.c popen.c

CFLAGS+= -DLOGIN_CAP -DPAM

DPADD=	${LIBCRON} ${LIBPAM} ${LIBUTIL}
LDADD=	${LIBCRON} ${MINUSLPAM} -lutil

WARNS?=	2

.include <bsd.prog.mk>
