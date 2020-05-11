# $FreeBSD: stable/11/usr.sbin/hyperv/tools/kvp/Makefile 319119 2017-05-29 10:15:41Z ngie $

.include <bsd.own.mk>

HV_KVP_DAEMON_DISTDIR?= ${SRCTOP}/contrib/hyperv/tools
.PATH: ${HV_KVP_DAEMON_DISTDIR}

PROG= 	hv_kvp_daemon
MAN= 	hv_kvp_daemon.8

CFLAGS+=	-I${SRCTOP}/sys/dev/hyperv/utilities

.include <bsd.prog.mk>
