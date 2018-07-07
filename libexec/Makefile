#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/Makefile,v 1.77 2007/07/03 12:46:04 mlaier Exp $
# $MidnightBSD$

.include <bsd.own.mk>

SUBDIR=	${_atf} \
	${_atrun} \
	${_comsat} \
	getty \
	${_hyperv} \
	${_mail.local} \
	${_makewhatis.local} \
	${_mknetid} \
	mport.check-fake \
	mport.check-for-older \
	mport.create \
	mport.delete \
	mport.fetch \
	mport.info \
	mport.init \
	mport.install \
	mport.list \
	mport.merge \
	mport.query \
	mport.update \
	mport.updepends \
	mport.version_cmp \
	msearch.index \
	${_pppoed} \
	revnetgroup \
	${_rlogind} \
	rpc.rquotad \
	rpc.rstatd \
	rpc.rusersd \
	rpc.rwalld \
	rpc.sprayd \
	${_rshd} \
	${_rtld-elf} \
	save-entropy \
	${_smrsh} \
	${_telnetd} \
	${_tests} \
	${_tftp-proxy} \
	ulog-helper \
	${_ypxfr}

.if ${MK_AT} != "no"
_atrun=		atrun
.endif

.if ${MK_BOOTPD} != "no"
SUBDIR+=	bootpd
.endif

.if ${MK_FINGER} != "no"
SUBDIR+=	fingerd
.endif

.if ${MK_FTP} != "no"
SUBDIR+=	ftpd
.endif

.if ${MK_MAIL} != "no"
_comsat=	comsat
.endif

.if ${MK_HYPERV} != "no"
_hyperv=	hyperv
.endif

.if ${MK_MAN_UTILS} != "no"
_makewhatis.local=	makewhatis.local
.endif

.if ${MK_NIS} != "no"
_mknetid=	mknetid
_ypxfr=		ypxfr
.endif

.if ${MK_NETGRAPH} != "no"
_pppoed=	pppoed
.endif

.if ${MK_PF} != "no"
_tftp-proxy=	tftp-proxy
.endif

.if !defined(NO_PIC) && !defined(NO_RTLD)
_rtld-elf=	rtld-elf
.endif

.if ${MK_RBOOTD} != "no"
SUBDIR+=	rbootd
.endif

.if ${MK_RCMDS} != "no"
_rlogind=	rlogind
_rshd=		rshd
.endif

.if ${MK_SENDMAIL} != "no"
_mail.local=	mail.local
_smrsh=		smrsh
.endif

.if ${MK_TALK} != "no"
SUBDIR+=	talkd
.endif

.if ${MK_TCP_WRAPPERS} != "no"
SUBDIR+=	tcpd
.endif

.if ${MK_TELNET} != "no"
_telnetd=	telnetd
.endif

.if ${MK_TFTP} != "no"
SUBDIR+=	tftpd
.endif

.if ${MK_TESTS} != "no"
_atf=		atf
_tests=		tests
.endif

.include <bsd.subdir.mk>
