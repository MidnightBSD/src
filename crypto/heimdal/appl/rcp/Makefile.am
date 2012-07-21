# $Id: Makefile.am,v 1.1.1.3 2012-07-21 15:09:09 laffer1 Exp $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_krb4)

bin_PROGRAMS = rcp

rcp_SOURCES  = rcp.c util.c rcp_locl.h extern.h

man_MANS = rcp.1

EXTRA_DIST = $(man_MANS)

LDADD = $(LIB_roken)
