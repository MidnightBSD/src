# $FreeBSD: release/7.0.0/etc/pam.d/Makefile 170890 2007-06-17 17:25:53Z yar $

NO_OBJ=

FILES=	README \
	atrun \
	cron \
	ftpd \
	gdm \
	imap \
	kde \
	login \
	other \
	passwd pop3 \
	rsh \
	sshd su system \
	telnetd \
	xdm

FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>
