#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $MidnightBSD: src/usr.bin/Makefile,v 1.42 2012/11/17 19:14:34 laffer1 Exp $

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
	awk \
	banner \
	basename \
	batt \
	brainfuck \
	brandelf \
	bsdiff \
	bzip2 \
	bzip2recover \
	cap_mkdb \
	cflow \
	cgraph \
	chat \
	checknr \
	chpass \
	cksum \
	${_clang} \
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
	deroff \
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
	fstat \
	fsync \
	ftp \
	gcore \
	gencat \
	getconf \
	getent \
	getopt \
	grep \
	gzip \
	head \
	hexdump \
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
	mt \
	ncal \
	netstat \
	newgrp \
	nfsstat \
	nice \
	nl \
	nohup \
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
	stdbuf \
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
	uniq \
	unzip \
	units \
	unvis \
	uudecode \
	uuencode \
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
	${_xstr} \
	xz \
	xzdec \
	${_yacc} \
	yes \
	${_ypcat} \
	${_ypmatch} \
	${_ypwhich}

.if ${MK_AT} != "no"
SUBDIR+=	at
.endif

.if ${MK_ATM} != "no"
SUBDIR+=	atm
.endif

.if ${MK_MAN_UTILS} != "no"
SUBDIR+=	catman
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

.if ${MK_BSD_CPIO} != "no"
SUBDIR+=	cpio
.endif

.if ${MK_CALENDAR} != "no"
SUBDIR+=	calendar
.endif

.if ${MK_CLANG} != "no"
_clang=		clang
.endif

.if ${MK_HESIOD} != "no"
SUBDIR+=	hesinfo
.endif

.if ${MK_ICONV} != "no"
_iconv=		iconv
_mkcsmapper=	mkcsmapper
_mkesdb=	mkesdb
.endif

.if ${MK_GROFF} != "no"
SUBDIR+=	vgrind
.endif

.if ${MK_OPENSSL} != "no"
SUBDIR+=	bc
SUBDIR+=	chkey
SUBDIR+=	dc
SUBDIR+=	newkey
.endif

.if ${MK_LOCATE} != "no"
SUBDIR+=	locate
.endif

.if ${MK_MAIL} != "no"
SUBDIR+=	biff
SUBDIR+=	from
SUBDIR+=	mail
SUBDIR+=	msgs
.endif

.if ${MK_MAKE} != "no"
SUBDIR+=	make
.endif

.if ${MK_MAN_UTILS} != "no"
_makewhatis=	makewhatis
_man=		man
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
