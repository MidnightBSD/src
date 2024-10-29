#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93

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
	nproc \
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
	timeout \
	uuidgen


SUBDIR.${MK_ASH}+=  sh                                                                                                                                      
SUBDIR.${MK_SENDMAIL}+=       rmail                                                                                                                                     
SUBDIR.${MK_TCSH}+=   csh                                                                                                                                               
SUBDIR.${MK_TESTS}+=  tests  

.include <bsd.arch.inc.mk>

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
