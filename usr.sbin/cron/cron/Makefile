# $FreeBSD: stable/11/usr.sbin/cron/cron/Makefile 275054 2014-11-25 16:57:27Z bapt $

PROG=	cron
MAN=	cron.8
SRCS=	cron.c database.c do_command.c job.c user.c popen.c

CFLAGS+= -DLOGIN_CAP -DPAM

LIBADD=	cron pam util

WARNS?=	2

.include <bsd.prog.mk>
