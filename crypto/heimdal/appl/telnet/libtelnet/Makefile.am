# $Id: Makefile.am,v 1.1.1.3 2012-07-21 15:09:08 laffer1 Exp $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += -I$(srcdir)/.. $(INCLUDE_krb4) $(INCLUDE_hcrypto)

noinst_LIBRARIES = libtelnet.a

libtelnet_a_SOURCES = \
	auth-proto.h	\
	auth.c		\
	auth.h		\
	enc-proto.h	\
	enc_des.c	\
	encrypt.c	\
	encrypt.h	\
	genget.c	\
	kerberos.c	\
	kerberos5.c	\
	misc-proto.h	\
	misc.c		\
	misc.h

EXTRA_DIST = krb4encpwd.c rsaencpwd.c spx.c
