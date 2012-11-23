#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
# $FreeBSD: src/usr.sbin/Makefile,v 1.332.2.3 2005/12/15 23:30:11 pjd Exp $
# $MidnightBSD: src/usr.sbin/Makefile,v 1.30 2012/11/17 02:20:56 laffer1 Exp $

.include <bsd.own.mk>

# XXX MISSING:		mkproto
SUBDIR+=accton \
	${_acpi} \
	adduser \
	amd \
	${_apm} \
	${_apmd} \
	${_arlcontrol} \
	arp \
	${_asf} \
	${_authpf} \
	${_boot0cfg} \
	bootparamd \
	bsdinstall \
	bsnmpd \
	${_btxld} \
	burncd \
	cdcontrol \
	chkgrp \
	chown \
	chroot \
	ckdist \
	${_cpucontrol} \
	cron \
	ctm \
	daemon \
	dconschat \
	devinfo \
	digictl \
	diskinfo \
	${_dnssec-dsfromkey} \
	${_dnssec-keyfromlabel} \
	${_dnssec-keygen} \
	${_dnssec-signzone} \
	edquota \
	${_eeprom} \
	extattr \
	extattrctl \
	${_faithd} \
	fdcontrol \
	fdformat \
	fdread \
	fdwrite \
	flowctl \
	${_ftp-proxy} \
	fwcontrol \
	getfmac \
	getpmac \
	gstat \
	ifmcstat \
	inetd \
	iostat \
	${_ip6addrctl} \
	ipfwpcap \
	${_IPXrouted} \
	jail \
	jexec \
	jls \
	kbdcontrol \
	kbdmap \
	${_keyserv} \
	${_kgmon} \
	${_kgzip} \
	kldxref \
	lmcconfig \
	${_lpr} \
	${_lptcontrol} \
	mailwrapper \
	makefs \
	manctl \
	memcontrol \
	mergemaster \
	mixer \
	${_mld6query} \
	mlxcontrol \
	mountd \
	${_mount_nwfs} \
	mount_portalfs \
	${_mount_smbfs} \
	moused \
	mport \
	${_mptable} \
	mptutil \
	mrouted \
	mtest \
	mtree \
	${_named} \
	${_named-checkconf} \
	${_named-checkzone} \
	${_named.reload} \
	${_ndiscvt} \
	${_ndp} \
	newsyslog \
	nfsd \
	ngctl \
	nghook \
	nologin \
	${_nscd} \
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
	pppd \
	pppstats \
	procctl \
	pstat \
	pw \
	pwd_mkdb \
	quot \
	quotaon \
	rarpd \
	raycontrol \
	rdate \
	repquota \
	${_rip6query} \
	rmt \
	${_rndc} \
	${_rndc-confgen} \
	${_route6d} \
	rpcbind \
	rpc.lockd \
	rpc.statd \
	rpc.umntall \
	${_rpc.yppasswdd} \
	${_rpc.ypupdated} \
	${_rpc.ypxfrd} \
	${_rrenumd} \
	${_rtadvd} \
	rtprio \
	${_rtsold} \
	rwhod \
	sa \
	sade \
	setfib \
	setfmac \
	setpmac \
	sensorsd \
	service \
	${_sicontrol} \
	smbmsg \
	snapinfo \
	${_spkrtest} \
	spray \
	syslogd \
	sysrc \
	tcpdchk \
	tcpdmatch \
	tcpdrop \
	tcpdump \
	timed \
	traceroute \
	${_traceroute6} \
	trpt \
	tzsetup \
	ugidfw \
	vidcontrol \
	vipw \
	wake \
	watch \
	watchdogd \
	${_wlconfig} \
	${_ypbind} \
	${_yp_mkdb} \
	${_yppoll} \
	${_yppush} \
	${_ypserv} \
	${_ypset} \
	zic \
	${_zzz}

.if ${MK_AUDIT} != "no"
SUBDIR+=	audit
SUBDIR+=	auditd
SUBDIR+=	auditreduce
SUBDIR+=	praudit
.endif

.if ${MK_BIND_DNSSEC} != "no" && ${MK_OPENSSL} != "no"
_dnssec-dsfromkey=	dnssec-dsfromkey
_dnssec-keyfromlabel=	dnssec-keyfromlabel
_dnssec-keygen=		dnssec-keygen
_dnssec-signzone=	dnssec-signzone
.endif
.if ${MK_BIND_NAMED} != "no"
_named=			named
_named-checkconf=	named-checkconf
_named-checkzone=	named-checkzone
_named.reload=		named.reload
_rndc=			rndc
_rndc-confgen=		rndc-confgen
.endif

.if ${MK_BLUETOOTH} != "no"
SUBDIR+=	bluetooth
.endif

.if ${MK_OPENSSL} != "no"
_keyserv=	keyserv
.endif

.if ${MK_PF} != "no"
_ftp-proxy=	ftp-proxy
.endif

.if ${MK_INET6} != "no"
_faithd=	faithd
_ip6addrctl=	ip6addrctl
_mld6query=	mld6query
_ndp=		ndp
_rip6query=	rip6query
_route6d=	route6d
_rrenumd=	rrenumd
_rtadvd=	rtadvd
_rtsold=	rtsold
_traceroute6=	traceroute6
.endif

.if ${MK_IPX} != "no"
_IPXrouted=	IPXrouted
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

.if ${MK_AUTHPF} != "no"
_authpf=	authpf
.endif

.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_pppctl=	pppctl
.endif

.if ${MK_LPR} != "no"
_lpr=		lpr
.endif

.if ${MK_NS_CACHING} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_nscd=		nscd
.endif
.endif

.if ${MK_SENDMAIL} != "no"
SUBDIR+=	editmap
SUBDIR+=	mailstats
SUBDIR+=	makemap
SUBDIR+=	praliases
SUBDIR+=	sendmail
.endif

.if ${MACHINE_ARCH} == "i386"
_apm=		apm
_apmd=		apmd
_asf=		asf
_btxld=		btxld
_cpucontrol=	cpucontrol
_kgmon=		kgmon
_kgzip=		kgzip
_lptcontrol=	lptcontrol
.if ${MK_NCP} != "no"
_mount_nwfs=	mount_nwfs
.endif
_mount_smbfs=	mount_smbfs
_mptable=	mptable
_ndiscvt=	ndiscvt
_pnpinfo=	pnpinfo
_sicontrol=	sicontrol
_spkrtest=	spkrtest
_zzz=		zzz
.if ${MK_ACPI} != "no"
_acpi=		acpi
.endif
_arlcontrol=	arlcontrol
_boot0cfg=	boot0cfg
_wlconfig=	wlconfig
.endif

# kgzip: builds, but missing support files
# mptable: broken (not 64 bit clean)
# pnpinfo: crashes (not really useful anyway)
.if ${MACHINE_ARCH} == "amd64"
.if ${MK_ACPI} != "no"
_acpi=		acpi
.endif
_asf=		asf
_boot0cfg=	boot0cfg
_btxld=		btxld
_cpucontrol=	cpucontrol
_kgmon=		kgmon
_lptcontrol=	lptcontrol
.if ${MK_NCP} != "no"
_mount_nwfs=	mount_nwfs
.endif
_mount_smbfs=	mount_smbfs
_mptable=	mptable
_ndiscvt=	ndiscvt
_sicontrol=	sicontrol
_spkrtest=	spkrtest
_zzz=		zzz
.endif

.if ${MACHINE_ARCH} == "sparc64"
_eeprom=	eeprom
_ofwdump=	ofwdump
.endif

.if ${MK_SYSINSTALL} != "no"
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
