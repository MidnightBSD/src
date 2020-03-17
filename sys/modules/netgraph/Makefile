# $Whistle: Makefile,v 1.5 1999/01/24 06:48:37 archie Exp $
# $FreeBSD: stable/11/sys/modules/netgraph/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

SUBDIR=	async \
	atm \
	atmllc \
	${_bluetooth} \
	bpf \
	bridge \
	car \
	cisco \
	deflate \
	device \
	echo \
	eiface \
	etf \
	ether \
	ether_echo \
	frame_relay \
	gif \
	gif_demux \
	hole \
	hub \
	iface \
	ip_input \
	ipfw \
	ksocket \
	l2tp \
	lmi \
	${_mppc} \
	nat \
	netflow \
	netgraph \
	one2many \
	patch \
	pipe \
	ppp \
	pppoe \
	pptpgre \
	pred1 \
	rfc1490 \
	socket \
	source \
	split \
	sppp \
	tag \
	tcpmss \
	tee \
	tty \
	UI \
	vjc \
	vlan

.if ${MK_BLUETOOTH} != "no" || defined(ALL_MODULES)
_bluetooth=	bluetooth
.endif

.if ${MK_CRYPT} != "no" && exists(${SYSDIR}/crypto/rc4/rc4.c)
_mppc=		mppc
.endif

.include <bsd.subdir.mk>
