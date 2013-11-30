#	$OpenBSD: Makefile,v 1.2 2004/09/28 15:10:51 millert Exp $
#       $MidnightBSD: src/usr.bin/sudo/sudo/Makefile,v 1.5 2007/08/19 01:59:21 archite Exp $

SUDO_DIR=       ${.CURDIR}/../../../contrib/sudo
AUTH_DIR=	${SUDO_DIR}/auth
.PATH:	${SUDO_DIR} ${AUTH_DIR}

PROG=	sudo
INSTALLFLAGS=	-S
SRCS=	audit.c check.c env.c getspwuid.c interfaces.c lbuf.c logging.c \
	memrchr.c \
	parse.c pwutil.c set_perms.c sudo.c sudo_edit.c sudo_nss.c \
	term.c tgetpass.c zero_bytes.c sudo_auth.c pam.c

CFLAGS+=	-I${SUDO_DIR} -I${AUTH_DIR}
LDADD+=         -lopie -lpam ${LIBSUDO} ${LIBUTIL}
MAN=	sudo.8 sudoers.5

BINMODE=	4555
BINDIR=	/usr/bin
LINKS= 	${BINDIR}/sudo ${BINDIR}/sudoedit
MLINKS=	sudo.8 sudoedit.8

.include <bsd.prog.mk>
