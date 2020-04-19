# $FreeBSD: stable/11/cddl/Makefile.inc 270358 2014-08-22 20:04:51Z delphij $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=	YES

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN

WARNS?=		6

# Do not lint the CDDL stuff. It is all externally maintained and
# lint output is wasteful noise here.

NO_LINT=
