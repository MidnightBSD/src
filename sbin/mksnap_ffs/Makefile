# $FreeBSD: release/7.0.0/sbin/mksnap_ffs/Makefile 111716 2003-03-02 01:50:33Z mckusick $

PROG=	mksnap_ffs
MAN=	mksnap_ffs.8

.if defined(NOSUID)
BINMODE=550
.else
BINMODE=4550
BINOWN=	root
.endif
BINGRP=	operator

.include <bsd.prog.mk>
