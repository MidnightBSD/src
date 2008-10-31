# $Id: Makefile.am,v 1.1.1.3 2008-10-31 20:45:58 laffer1 Exp $
# $FreeBSD: src/crypto/heimdal/appl/su/Makefile.am,v 1.2.10.1 2007/10/29 21:39:03 peter Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) $(INCLUDE_des)

bin_PROGRAMS = su
bin_SUIDS = su
su_SOURCES = su.c

LDADD = $(LIB_kafs) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)
