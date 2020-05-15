#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: stable/10/share/doc/Makefile 291792 2015-12-04 18:14:31Z bdrewery $

.include <src.opts.mk>

SUBDIR=	${_IPv6} \
	${_atf} \
	legal \
	${_llvm} \
	${_pjdfstest}

.if ${MK_TESTS} != "no"
_atf= atf
_pjdfstest= pjdfstest
.endif

.if ${MK_CLANG} != "no"
_llvm=	llvm
.endif

.if ${MK_INET6} != "no"
_IPv6=	IPv6
.endif

SUBDIR_PARALLEL=

# Default output format for troff documents is ascii.
# To generate postscript versions of troff documents, use:
#  make PRINTERDEVICE=ps

.include <bsd.subdir.mk>
