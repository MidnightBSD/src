#	$OpenBSD: Makefile,v 1.2 2004/09/28 15:10:51 millert Exp $
#       $MidnightBSD$

SUDO_DIR=       ${.CURDIR}/../../../contrib/sudo
AUTH_DIR=	${SUDO_DIR}/auth
.PATH:	${SUDO_DIR} ${AUTH_DIR}

PROG=	sudo
INSTALLFLAGS=	-S
SRCS=	check.c closefrom.c env.c getspwuid.c interfaces.c logging.c \
	parse.c set_perms.c sudo.c tgetpass.c zero_bytes.c sudo_auth.c \
	pam.c sudo_edit.c

CFLAGS+=	-I${SUDO_DIR} -I${AUTH_DIR}
LDADD+=         -L${.OBJDIR}/../lib -lopie -lpam -lsudo -lutil
DPADD+=         -L${.OBJDIR}/../lib/libsudo.a
MAN=	sudo.8 sudoers.5

BINMODE=	4555
BINDIR=	/usr/bin
LINKS= 	${BINDIR}/sudo ${BINDIR}/sudoedit
MLINKS=	sudo.8 sudoedit.8

afterinstall:
	cp -n ${SUDO_DIR}/sudoers /etc/sudoers
	chmod 0440 /etc/sudoers

.include <bsd.prog.mk>
