#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile,v 1.25 2005/03/02 11:53:21 trhodes Exp $

.include <src.opts.mk>

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
	sleep \
	stty \
	sync \
	test \
	uuidgen


SUBDIR.${MK_ASH}+=  sh                                                                                                                                      
SUBDIR.${MK_RCMDS}+=  rcp                                                                                                                                               
SUBDIR.${MK_SENDMAIL}+=       rmail                                                                                                                                     
SUBDIR.${MK_TCSH}+=   csh                                                                                                                                               
SUBDIR.${MK_TESTS}+=  tests  

.include <bsd.arch.inc.mk>

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
