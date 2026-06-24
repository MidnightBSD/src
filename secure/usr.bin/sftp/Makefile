
.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-common.c sftp-glob.c sftp-usergroup.c \
	progressmeter.c
PACKAGE=	ssh
CFLAGS+=	-I${SSHDIR}/openbsd-compat

LIBADD=	ssh edit

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
