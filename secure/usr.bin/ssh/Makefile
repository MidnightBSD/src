# $MidnightBSD: src/secure/usr.bin/ssh/Makefile,v 1.3 2006/10/10 18:52:37 laffer1 Exp $
# $FreeBSD: src/secure/usr.bin/ssh/Makefile,v 1.31 2005/06/07 09:31:27 des Exp $
#

PROG=	ssh
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MAN=	ssh.1 ssh_config.5
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c \
	gss-genr.c

DPADD=	${LIBSSH} ${LIBUTIL} ${LIBZ}
LDADD=	-lssh -lutil -lz

.if !defined(NO_KERBEROS)
CFLAGS+= -DGSSAPI -DHAVE_GSSAPI_H=1 -DKRB5 -DHEIMDAL
DPADD+=	 ${LIBGSSAPI} ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBMD} ${LIBROKEN}
LDADD+=	 -lgssapi -lkrb5 -lasn1 -lcom_err -lmd -lroken
.endif

if defined(X11BASE) || defined(LOCALBASE)
# Recommended /etc/make.conf setting is X11BASE=${LOCALBASE} for x.org
# 7.x upgrade on <= 6.2, but LOCALBASE has moved out of scope of src/
# so we need to provide the default for users with old make.conf
# settings.
LOCALBASE?=     /usr/local
  	 
# Users may override either LOCALBASE or X11BASE to move the location
# of xauth
X11BASE?=       ${LOCALBASE}
CFLAGS+= -DXAUTH_PATH=\"${LOCALBASE}/bin/xauth\"
.endif

DPADD+=	${LIBCRYPT} ${LIBCRYPTO}
LDADD+=	-lcrypt -lcrypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
