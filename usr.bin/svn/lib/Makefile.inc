# $FreeBSD: release/10.0.0/usr.bin/svn/lib/Makefile.inc 251886 2013-06-18 02:53:45Z peter $

.if !defined(SVNDIR)

SVNDIR=		${.CURDIR}/../../../../contrib/subversion/subversion
APRU=		${.CURDIR}/../../../../contrib/apr-util
APR=		${.CURDIR}/../../../../contrib/apr
SQLITE=		${.CURDIR}/../../../../contrib/sqlite3
SERF=		${.CURDIR}/../../../../contrib/serf

WARNS?=		0	# definitely not ready

.if exists(${.CURDIR}/../../../Makefile.inc)
.include "${.CURDIR}/../../../Makefile.inc"
.endif

.endif
