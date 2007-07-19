#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile,v 1.25 2005/03/02 11:53:21 trhodes Exp $
# $MidnightBSD: src/bin/Makefile,v 1.3 2006/09/04 21:00:53 archite Exp $

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
	ksh \
	ln \
	ls \
	mkdir \
	mv \
	pax \
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

.if !defined(NO_RCMDS)
_rcp=	rcp
.endif

.if !defined(NO_SENDMAIL)
_rmail=	rmail
.endif

.if !defined(NO_TCSH)
_csh=	csh
.endif

.include <bsd.subdir.mk>
