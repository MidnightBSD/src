#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile,v 1.25 2005/03/02 11:53:21 trhodes Exp $
# $MidnightBSD$

.include <bsd.own.mk>

SUBDIR= cat \
	chflags \
	chio \
	chmod \
	cp \
	cpdup \
	date \
	dd \
	df \
	domainname \
	echo \
	ed \
	expr \
	getfacl \
	hostname \
	kenv \
	kill \
	ln \
	ls \
	midnightbsd-version \
	mined \
	mkdir \
	mksh \
	mv \
	notty \
	pax \
	pkill \
	ps \
	pwait \
	pwd \
	realpath \
	rm \
	rmdir \
	setfacl \
	${_sh} \
	sleep \
	stty \
	sync \
	test \
	uuidgen

.if ${MK_ASH} != "no"
_sh=	sh
.endif

.if ${MK_RCMDS} != "no"
SUBDIR+=	rcp
.endif

.if ${MK_SENDMAIL} != "no"
SUBDIR+=	rmail
.endif

.if ${MK_TCSH} != "no"
SUBDIR+=	csh
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.arch.inc.mk>

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
