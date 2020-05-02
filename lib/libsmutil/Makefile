# $FreeBSD: stable/11/lib/libsmutil/Makefile 313538 2017-02-10 07:32:40Z ngie $

PACKAGE=lib${LIB}
SENDMAIL_DIR=${SRCTOP}/contrib/sendmail
.PATH:	${SENDMAIL_DIR}/libsmutil

CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -I.
CFLAGS+=-DNEWDB -DNIS -DMAP_REGEX -DNOT_SENDMAIL

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}

WARNS?=	2

LIB=	smutil

SRCS+=	sm_os.h
SRCS+=	debug.c err.c lockfile.c safefile.c snprintf.c cf.c
CLEANFILES+=sm_os.h

INTERNALLIB=

sm_os.h: ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h .NOMETA
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.lib.mk>
