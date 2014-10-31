## $NetBSD: Makefile.am,v 1.3 2013/05/05 13:06:37 lukem Exp $

ACLOCAL_AMFLAGS = -I buildaux

SUBDIRS = libnetbsd

if USE_LIBEDIT
SUBDIRS += libedit
endif

SUBDIRS += src

EXTRA_DIST = \
	todo \
	tnftp.h
