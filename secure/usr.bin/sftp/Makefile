# $MidnightBSD: src/secure/usr.bin/sftp/Makefile,v 1.5 2009/05/02 18:01:22 laffer1 Exp $

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-common.c sftp-glob.c progressmeter.c \
	roaming_common.o
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ} ${LIBEDIT} ${LIBNCURSES}
LDADD=	-lssh -lcrypt -lcrypto -lz -ledit -lncurses

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
