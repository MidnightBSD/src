#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.sbin/kgmon/Makefile 275054 2014-11-25 16:57:27Z bapt $

PROG=	kgmon
MAN=	kgmon.8
WARNS?=	2

# This program may safely be run setuid-root to allow non-root
# users to start, stop, and reset profiling buffers.
#
#BINOWN=root
#BINMODE=4555

LIBADD=	kvm

.include <bsd.prog.mk>
