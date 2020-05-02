# $FreeBSD: stable/11/kerberos5/libexec/kdigest/Makefile 288200 2015-09-24 23:23:58Z bdrewery $

PROG=	kdigest
MAN=	kdigest.8
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
LIBADD=	krb5 heimntlm roken crypto edit sl vers
SRCS=	kdigest.c \
	kdigest-commands.c \
	kdigest-commands.h

CLEANFILES=	kdigest-commands.h kdigest-commands.c

kdigest-commands.h: kdigest-commands.in
	${SLC} ${.ALLSRC:M*.in}

kdigest-commands.c: kdigest-commands.h

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
