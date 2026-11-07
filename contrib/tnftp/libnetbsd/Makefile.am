## $NetBSD: Makefile.am,v 1.4 2021/08/25 00:59:48 lukem Exp $

noinst_LTLIBRARIES = libnetbsd.la

libnetbsd_la_SOURCES = libnetbsd.c

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_builddir)

libnetbsd_la_LIBADD = $(LTLIBOBJS)

EXTRA_DIST = \
	fseeko.c \
	ftpglob.h \
	ftpvis.h
