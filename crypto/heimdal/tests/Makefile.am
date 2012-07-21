# $Id: Makefile.am,v 1.1.1.1 2012-07-21 15:09:06 laffer1 Exp $

include $(top_srcdir)/Makefile.am.common

SUBDIRS = db kdc gss ldap can java

if ENABLE_SHARED
if HAVE_DLOPEN
SUBDIRS += plugin
endif
endif
