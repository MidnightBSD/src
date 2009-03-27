#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile,v 1.25 2005/03/02 11:53:21 trhodes Exp $
# $MidnightBSD: src/bin/Makefile,v 1.9 2008/11/11 22:40:37 laffer1 Exp $

.include <bsd.own.mk>

SUBDIR= cat \
	chflags \
	chio \
	chmod \
	cp \
	cpdup \
	${_csh} \
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
	mined \
	mkdir \
	mksh \
	mv \
	notty \
	pax \
	pkill \
	ps \
	pwd \
	${_rcp} \
	realpath \
	rm \
	${_rmail} \
	rmdir \
	setfacl \
	sh \
	sleep \
	stty \
	sync \
	test

.if ${MK_RCMDS} != "no"
_rcp=	rcp
.endif

.if ${MK_SENDMAIL} != "no"
_rmail=	rmail
.endif

.if ${MK_TCSH} != "no"
_csh=	csh
.endif

.include <bsd.subdir.mk>
