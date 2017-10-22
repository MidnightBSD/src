# $FreeBSD: stable/9/sys/modules/kgssapi_krb5/Makefile 231702 2012-02-14 19:36:35Z dim $

.PATH: ${.CURDIR}/../../kgssapi/krb5
KMOD=	kgssapi_krb5

SRCS=	krb5_mech.c \
	kcrypto.c \
	kcrypto_des.c \
	kcrypto_des3.c \
	kcrypto_aes.c \
	kcrypto_arcfour.c \
	opt_inet6.h

SRCS+=		kgss_if.h gssd.h
MFILES=		kgssapi/kgss_if.m
CLEANFILES=	gssd.h

S=		${.CURDIR}/../..

gssd.h:		$S/kgssapi/gssd.x
	RPCGEN_CPP=${CPP:Q} rpcgen -hM $S/kgssapi/gssd.x | grep -v pthread.h > gssd.h

.include <bsd.kmod.mk>
