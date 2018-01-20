#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/doc/Makefile,v 1.19 2004/09/25 09:11:39 dougb Exp $
# $MidnightBSD$

.include <bsd.own.mk>

SUBDIR=	IPv6 legal ${_llvm} ${_roffdocs}

.if ${MK_CLANG} != "no"
_llvm=	llvm
.endif

# Default output format for troff documents is ascii.
# To generate postscript versions of troff documents, use:
#  make PRINTERDEVICE=ps

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
