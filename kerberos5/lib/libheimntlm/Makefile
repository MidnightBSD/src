# $FreeBSD: stable/9/kerberos5/lib/libheimntlm/Makefile 225811 2011-09-27 18:40:13Z stas $

LIB=	heimntlm
LDFLAGS=	-Wl,--no-undefined
LDADD=	-lcrypto -lkrb5
DPADD=	${LIBCRYPTO} ${LIBKRB5}
SRCS=	ntlm.c
INCS=	heimntlm.h heimntlm-protos.h
CFLAGS+=-I${KRB5DIR}/lib/ntlm
VERSION_MAP= ${KRB5DIR}/lib/ntlm/version-script.map

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ntlm
