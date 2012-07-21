# $Id: Makefile.am,v 1.1.1.3 2012-07-21 15:09:07 laffer1 Exp $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_krb4)

lib_LIBRARIES = @EXTRA_LIB45@

EXTRA_LIBRARIES = lib45.a

lib45_a_SOURCES = get_ad_tkt.c mk_req.c 45_locl.h
