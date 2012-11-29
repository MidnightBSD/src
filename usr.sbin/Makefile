#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
# $MidnightBSD: src/usr.sbin/Makefile,v 1.31 2012/11/23 05:56:02 laffer1 Exp $

.include <bsd.own.mk>

# XXX MISSING:		mkproto, mptutil
SUBDIR=	adduser \
	arp \
	bootparamd \
	bsdinstall \
	bsnmpd \
	burncd \
	cdcontrol \
	chkgrp \
	chown \
	chroot \
	ckdist \
	cron \
	ctm \
	daemon \
	dconschat \
	devinfo \
	digictl \
	diskinfo \
	extattr \
	extattrctl \
	flowctl \
	fwcontrol \
	getfmac \
	getpmac \
	gstat \
	ifmcstat \
	inetd \
	iostat \
	ipfwpcap \
	jail \
	jexec \
	jls \
	kbdcontrol \
	kbdmap \
	${_keyserv} \
	kldxref \
	lmcconfig \
	mailwrapper \
	makefs \
	manctl \
	memcontrol \
	mergemaster \
	mixer \
	mlxcontrol \
	mountd \
	${_mount_nwfs} \
	mount_portalfs \
	mount_smbfs \
	moused \
	mport \
	${_mptable} \
	mrouted \
	mtest \
	mtree \
	${_ndiscvt} \
	newsyslog \
	nfsd \
	ngctl \
	nghook \
	nologin \
	ntpd \
	${_ofwdump} \
	pciconf \
	periodic \
	pmccontrol \
	pmcstat \
	${_pnpinfo} \
	powerd \
	ppp \
	${_pppctl} \
	procctl \
	pstat \
	pw \
	pwd_mkdb \
	quot \
	rarpd \
	raycontrol \
	rdate \
	rmt \
	rpcbind \
	rpc.lockd \
	rpc.statd \
	rpc.umntall \
	${_rpc.yppasswdd} \
	${_rpc.ypupdated} \
	${_rpc.ypxfrd} \
	rtprio \
	setfib \
	setfmac \
	setpmac \
	sensorsd \
	service \
	services_mkdb \
	smbmsg \
	snapinfo \
	spray \
	syslogd \
	sysrc \
	tcpdchk \
	tcpdmatch \
	tcpdrop \
	tcpdump \
	timed \
	traceroute \
	trpt \
	tzsetup \
	ugidfw \
	vidcontrol \
	vipw \
	wake \
	watch \
	watchdogd \
	${_ypbind} \
	${_yp_mkdb} \
	${_yppoll} \
	${_yppush} \
	${_ypserv} \
	${_ypset} \
	zic

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
SUBDIR+=	accton
SUBDIR+=	sa
.endif

.if ${MK_AMD} != "no"
SUBDIR+=	amd
.endif

.if ${MK_AUDIT} != "no"
SUBDIR+=	audit
SUBDIR+=	auditd
SUBDIR+=	auditreduce
SUBDIR+=	praudit
.endif

.if ${MK_AUTHPF} != "no"
SUBDIR+=	authpf
.endif

.if ${MK_BIND_DNSSEC} != "no" && ${MK_OPENSSL} != "no"
SUBDIR+=	dnssec-dsfromkey
SUBDIR+=	dnssec-keyfromlabel
SUBDIR+=	dnssec-keygen
SUBDIR+=	dnssec-signzone
.endif
.if ${MK_BIND_NAMED} != "no"
SUBDIR+=	named
SUBDIR+=	named-checkconf
SUBDIR+=	named-checkzone
SUBDIR+=	named.reload
SUBDIR+=	rndc
SUBDIR+=	rndc-confgen
.endif

.if ${MK_BLUETOOTH} != "no"
SUBDIR+=	bluetooth
.endif

.if ${MK_OPENSSL} != "no"
_keyserv=	keyserv
.endif

.if ${MK_FLOPPY} != "no"
SUBDIR+=	fdcontrol
SUBDIR+=	fdformat
SUBDIR+=	fdread
SUBDIR+=	fdwrite
.endif

.if ${MK_PF} != "no"
SUBDIR+=	ftp-proxy
.endif

.if ${MK_INET6} != "no"
SUBDIR+=	faithd
SUBDIR+=	ip6addrctl
SUBDIR+=	mld6query
SUBDIR+=	ndp
SUBDIR+=	rip6query
SUBDIR+=	route6d
SUBDIR+=	rrenumd
SUBDIR+=	rtadvd
SUBDIR+=	rtsold
SUBDIR+=	traceroute6
.endif

.if ${MK_IPX} != "no"
SUBDIR+=	IPXrouted
.endif

.if ${MK_NIS} != "no"
_rpc.yppasswdd=	rpc.yppasswdd
_rpc.ypupdated=	rpc.ypupdated
_rpc.ypxfrd=	rpc.ypxfrd
_ypbind=	ypbind
_yp_mkdb=	yp_mkdb
_yppoll=	yppoll
_yppush=	yppush
_ypserv=	ypserv
_ypset=		ypset
.endif

.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_pppctl=	pppctl
.endif

.if ${MK_LPR} != "no"
SUBDIR+=	lpr
.endif

.if ${MK_NS_CACHING} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
SUBDIR+=	nscd
.endif
.endif

.if ${MK_QUOTAS} != "no"
SUBDIR+=	edquota
SUBDIR+=	quotaon
SUBDIR+=	repquota
.endif

.if ${MK_RCMDS} != "no"
SUBDIR+=	rwhod
.endif

.if ${MK_SENDMAIL} != "no"
SUBDIR+=	editmap
SUBDIR+=	mailstats
SUBDIR+=	makemap
SUBDIR+=	praliases
SUBDIR+=	sendmail
.endif

.if ${MK_SYSINSTALL} != "no"
SUBDIR+=	sade
SUBDIR+=	sysinstall
.endif

.if ${MK_TOOLCHAIN} != "no"
SUBDIR+=	config
SUBDIR+=	crunch
.endif

.if ${MK_USB} != "no"
SUBDIR+=	uathload
SUBDIR+=	uhsoctl
SUBDIR+=	usbconfig
SUBDIR+=	usbdump
.endif

.if ${MK_UTMPX} != "no"
SUBDIR+=	ac
SUBDIR+=	lastlogin
SUBDIR+=	utxrm
.endif

.if ${MK_WIRELESS} != "no"
SUBDIR+=	ancontrol
SUBDIR+=	wlandebug
SUBDIR+=	wpa
.endif

.include <bsd.arch.inc.mk>

SUBDIR:=	${SUBDIR:O}

.include <bsd.subdir.mk>
