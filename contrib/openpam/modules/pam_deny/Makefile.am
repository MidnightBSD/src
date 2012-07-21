# $Id: Makefile.am,v 1.1.1.2 2012-07-21 14:57:34 laffer1 Exp $

pkglibdir = @OPENPAM_MODULES_DIR@
INCLUDES = -I$(top_srcdir)/include

pkglib_LTLIBRARIES = pam_deny.la

pam_deny_la_SOURCES = pam_deny.c
pam_deny_la_LDFLAGS = -no-undefined -module -version-info @LIB_MAJ@
pam_deny_la_LIBADD = $(top_builddir)/lib/libpam.la
