#	@(#)Makefile	8.2 (Berkeley) 9/21/96

SENDMAIL_DIR= ${SRCTOP}/contrib/sendmail
.PATH: ${SENDMAIL_DIR}/mailstats

PROG=	mailstats
SRCS=	mailstats.c
MAN=	mailstats.8

CFLAGS+= -I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -I.
CFLAGS+= -DNOT_SENDMAIL

WARNS?=	2

LIBADD=	sm smutil

SRCS+=	sm_os.h
CLEANFILES+=sm_os.h

# User customizations to the sendmail build environment
CFLAGS+= ${SENDMAIL_CFLAGS}
DPADD+=	${SENDMAIL_DPADD}
LDADD+=	${SENDMAIL_LDADD}
LDFLAGS+= ${SENDMAIL_LDFLAGS}

sm_os.h: ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h .NOMETA
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
