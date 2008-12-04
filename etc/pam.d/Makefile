# $MidnightBSD: src/etc/pam.d/Makefile,v 1.3 2007/08/18 07:40:09 laffer1 Exp $
# $FreeBSD: src/etc/pam.d/Makefile,v 1.13 2005/06/10 20:52:36 nectar Exp $

NO_OBJ=
FILES=	README atrun cron ftpd gdm imap kde login other passwd pop3 \
	rsh sshd su system telnetd xdm
FILESDIR=	/etc/pam.d
FILESMODE=	644
FILESMODE_README=	444
LINKS=	${FILESDIR}/ftpd ${FILESDIR}/ftp

.include <bsd.prog.mk>
