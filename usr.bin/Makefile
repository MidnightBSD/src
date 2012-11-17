#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $MidnightBSD: src/usr.bin/Makefile,v 1.41 2012/10/29 20:54:16 laffer1 Exp $

.include <bsd.own.mk>

# XXX MISSING:		diction graph learn plot
#			spline struct xsend
# XXX Use GNU versions: apropos diff ld man patch whatis
# Moved to secure: bdes
#

SUBDIR=	alias \
	apply \
	asa \
	asmgraph \
	at \
	awk \
	banner \
	basename \
	batt \
	bc \
	biff \
	brainfuck \
	brandelf \
	bsdiff \
	bzip2 \
	bzip2recover \
	cap_mkdb \
	${_catman} \
	cflow \
	cgraph \
	chat \
	checknr \
	${_chkey} \
	chpass \
	cksum \
	cmp \
	col \
	colcrt \
	colldef \
	colrm \
	column \
	comm \
	compress \
	cpio \
	cpufreq \
	cpuset \
	csplit \
	cut \
	dc \
	deroff \
	${_dig} \
	dirname \
	du \
	ee \
	elf2aout \
	elfdump \
	enigma \
	env \
	expand \
	false \
	fetch \
	file \
	find \
	finger \
	fmt \
	fold \
	from \
	fstat \
	fsync \
	ftp \
	gcore \
	gencat \
	getconf \
	getent \
	getopt \
	${_gprof} \
	grep \
	gzip \
	head \
	hexdump \
	${_host} \
	${_iconv} \
	id \
	ipcrm \
	ipcs \
	join \
	jot \
	kdump \
	keylogin \
	keylogout \
	killall \
	ktrace \
	ktrdump \
	lam \
	lastcomm \
	ldd \
	leave \
	less \
	lessecho \
	lesskey \
	${_lex} \
	limits \
	locale \
	locate \
	lock \
	lockf \
	logger \
	login \
	logins \
	logname \
	look \
	lorder \
	lsvfs \
	m4 \
	mail \
	make \
	makewhatis \
	mesg \
	minigzip \
 	ministat \
 	${_mkcsmapper} \
 	${_mkesdb} \
	mkdep \
	mkfifo \
	mklocale \
	mkstr \
	mktemp \
	mkuzip \
	msearch \
	msgs \
	mt \
	ncal \
	netstat \
	newgrp \
	${_newkey} \
	nfsstat \
	nice \
	nl \
	nohup \
	${_nslookup} \
	${_nsupdate} \
	opieinfo \
	opiekey \
	opiepasswd \
	pagesize \
	passwd \
	paste \
	pathchk \
	perl \
	pr \
	printenv \
	printf \
	progress \
	quota \
	renice \
	rev \
	rpcgen \
	rpcinfo \
	rs \
	rup \
	rusers \
	rwall \
	script \
	sdiff \
	sed \
	seq \
	shar \
	showmount \
	sockstat \
	sort \
	spell \
	split \
	sqlite3 \
	stat \
	su \
	sudo \
	systat \
	tabs \
	tail \
	talk \
	tar \
	tcopy \
	tee \
	tftp \
	time \
	tip \
	top \
	touch \
	tput \
	tr \
	true \
	truncate \
	truss \
	tset \
	tsort \
	tty \
	ul \
	uname \
	unexpand \
	unifdef \
	uniq \
	units \
	unvis \
	uudecode \
	uuencode \
	unzip \
	vgrind \
	vi \
	vis \
	vmstat \
	w \
	wall \
	wc \
	what \
	whereis \
	which \
	whois \
	window \
	write \
	xargs \
	xinstall \
	${_xlint} \
	xz \
	xzdec \
	${_yacc} \
	yes 

.if ${MK_ATM} != "no"
SUBDIR+=	atm
.endif

.if ${MK_MAN} != "no"
_catman=	catman
.endif

.if ${MK_BIND_UTILS} != "no"
SUBDIR+=	dig
SUBDIR+=	host
SUBDIR+=	nslookup
SUBDIR+=	nsupdate
.endif

.if ${MK_BLUETOOTH} != "no"
SUBDIR+=	bluetooth
.endif

.if ${MK_CALENDAR} != "no"
SUBDIR+=	calendar
.endif

.if ${MK_OPENSSL} != "no"
_chkey=		chkey
_newkey=	newkey
.endif

.if ${MK_HESIOD} != "no"
SUBDIR+=	hesinfo
.endif

.if ${MK_ICONV} != "no"
_iconv=		iconv
_mkcsmapper=	mkcsmapper
_mkesdb=	mkesdb
.endif

.if ${MK_NETCAT} != "no"
SUBDIR+=	nc
.endif

.if ${MK_NIS} != "no"
SUBDIR+=	ypcat
SUBDIR+=	ypmatch
SUBDIR+=	ypwhich
.endif

.if ${MK_QUOTAS} != "no"
SUBDIR+=	quota
.endif

.if ${MK_RCMDS} != "no"
SUBDIR+=	rlogin
SUBDIR+=	rsh
SUBDIR+=	ruptime
SUBDIR+=	rwho
.endif

.if ${MK_SENDMAIL} != "no"
SUBDIR+=	vacation
.endif

.if ${MK_TELNET} != "no"
SUBDIR+=	telnet
.endif

.if ${MK_TEXTPROC} != "no"
SUBDIR+=	checknr
SUBDIR+=	colcrt
SUBDIR+=	ul
.endif

.if ${MK_TOOLCHAIN} != "no"
SUBDIR+=	ar
SUBDIR+=	c89
SUBDIR+=	c99
SUBDIR+=	compile_et
SUBDIR+=	ctags
SUBDIR+=	file2c
SUBDIR+=	gprof
SUBDIR+=	indent
SUBDIR+=	lex
SUBDIR+=	mkstr
SUBDIR+=	rpcgen
SUBDIR+=	unifdef
SUBDIR+=	xlint
SUBDIR+=	xstr
SUBDIR+=	yacc
.endif

.if ${MK_USB} != "no"
SUBDIR+=	usbhidaction
SUBDIR+=	usbhidctl
.endif

.if ${MK_UTMPX} != "no"
SUBDIR+=	last
SUBDIR+=	users
SUBDIR+=	who
SUBDIR+=	wtmpcvt
.endif

.include <bsd.arch.inc.mk>

SUBDIR:=	${SUBDIR:O}

.include <bsd.subdir.mk>
