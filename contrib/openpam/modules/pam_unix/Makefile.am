# $Id: Makefile.am,v 1.1.1.2 2012-07-21 14:57:34 laffer1 Exp $

pkglibdir = @OPENPAM_MODULES_DIR@
INCLUDES = -I$(top_srcdir)/include

if WITH_PAM_UNIX
pkglib_LTLIBRARIES = pam_unix.la

pam_unix_la_SOURCES = pam_unix.c
pam_unix_la_LDFLAGS = -no-undefined -module -version-info @LIB_MAJ@ @CRYPT_LIBS@
pam_unix_la_LIBADD = $(top_builddir)/lib/libpam.la
endif
