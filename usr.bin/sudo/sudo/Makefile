#	$OpenBSD: Makefile,v 1.2 2004/09/28 15:10:51 millert Exp $
#       $MidnightBSD: src/usr.bin/sudo/sudo/Makefile,v 1.6 2010/03/10 03:40:48 laffer1 Exp $

SUDO_DIR=       ${.CURDIR}/../../../contrib/sudo
AUTH_DIR=	${SUDO_DIR}/auth
.PATH:	${SUDO_DIR} ${AUTH_DIR}

PROG=	sudo
INSTALLFLAGS=	-S
SRCS=	audit.c boottime.c check.c env.c exec.c exec_pty.c getspwuid.c get_pty.c \
	interfaces.c iolog.c lbuf.c logging.c memrchr.c \
	parse.c parse_args.c pwutil.c set_perms.c sudo.c sudo_edit.c sudo_nss.c \
	term.c tgetpass.c timestr.c zero_bytes.c sudo_auth.c pam.c

CFLAGS+=	-I${SUDO_DIR} -I${AUTH_DIR}
LDADD+=         -lopie -lpam ${LIBSUDO} ${LIBUTIL} ${LIBZ}
MAN=	sudo.8 sudoers.5

BINMODE=	4555
BINDIR=	/usr/bin
LINKS= 	${BINDIR}/sudo ${BINDIR}/sudoedit
MLINKS=	sudo.8 sudoedit.8

.include <bsd.prog.mk>
