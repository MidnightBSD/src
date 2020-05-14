# $FreeBSD: stable/11/usr.bin/svn/lib/Makefile.inc 319186 2017-05-30 04:27:05Z ngie $

.if !defined(SVNDIR)

SVNDIR=		${SRCTOP}/contrib/subversion/subversion
APRU=		${SRCTOP}/contrib/apr-util
APR=		${SRCTOP}/contrib/apr
SQLITE=		${SRCTOP}/contrib/sqlite3
SERF=		${SRCTOP}/contrib/serf

WARNS?=		0	# definitely not ready

.if exists(${.CURDIR}/../../../Makefile.inc)
.include "${.CURDIR}/../../../Makefile.inc"
.endif

.endif
