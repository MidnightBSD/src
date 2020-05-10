#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: stable/11/libexec/mail.local/Makefile 321269 2017-07-20 00:58:19Z ngie $

PACKAGE=sendmail
SENDMAIL_DIR=${SRCTOP}/contrib/sendmail
.PATH:	${SENDMAIL_DIR}/mail.local

PROG=	mail.local
SRCS=	mail.local.c
MAN=	mail.local.8
CFLAGS+=-I${SENDMAIL_DIR}/include -I.

WARNS?=	2
WFORMAT=0

LIBADD=	sm

SRCS+=	sm_os.h
CLEANFILES+=sm_os.h

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}
DPADD+=${SENDMAIL_DPADD}
LDADD+=${SENDMAIL_LDADD}
LDFLAGS+=${SENDMAIL_LDFLAGS}

sm_os.h: ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h .NOMETA
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
