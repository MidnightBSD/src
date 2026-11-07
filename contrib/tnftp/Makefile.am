## $NetBSD: Makefile.am,v 1.4 2020/07/05 09:37:53 lukem Exp $

ACLOCAL_AMFLAGS = -I buildaux

SUBDIRS = libnetbsd

if WITH_LOCAL_LIBEDIT
SUBDIRS += libedit
endif

SUBDIRS += src

EXTRA_DIST = \
	todo \
	tnftp.h
