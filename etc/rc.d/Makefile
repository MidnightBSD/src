# $MidnightBSD: src/etc/rc.d/Makefile,v 1.9 2008/04/28 05:22:21 laffer1 Exp $ 

.include <bsd.own.mk>

FILES=	DAEMON FILESYSTEMS LOGIN NETWORKING SERVERS \
	abi accounting addswap adjkerntz amd \
	apm apmd archdep atm1 atm2 atm3 auditd auto_linklocal \
	bgfsck bluetooth bootparams bridge bsnmpd bthidd \
	ccd cleanvar cleartmp cron \
	devd devfs dhclient \
	dmesg dumpon \
	encswap \
	firstboot fsck ftp-proxy ftpd \
	gbde geli geli2 \
	hcsecd \
	hostapd hostname \
	ike \
	inetd initrandom \
	ip6addrctl ip6fw ipfilter ipfs ipfw ipmon \
	ipnat ipsec ipxrouted isdnd \
	jail \
	kadmind kerberos keyserv kldxref kpasswdd \
	ldconfig local localpkg lpd \
	mixer motd mountcritlocal mountcritremote \
	mountd moused mroute6d mrouted msgs \
	named natd netif netoptions \
	network_ipv6 newsyslog nfsclient nfsd \
	nfslocking nfsserver nisdomain nsswitch ntpd \
	othermta \
	pccard pf pflog pfsync \
	powerd power_profile ppp pppoed pwcheck \
	quota \
	ramdisk ramdisk-own random rarpd rdate resolv root \
	route6d routed routing rpcbind rtadvd rwho \
	savecore sdpd securelevel sendmail \
	serial sound sppp swap1 \
	syscons sysctl syslogd \
	timed tmp \
	ugidfw \
	var virecover \
	watchdogd wpa_supplicant \
	ypbind yppasswdd ypserv \
	ypset ypupdated ypxfrd \
	zfs

.if ${MK_OPENSSH} != "no"
FILES+=	sshd
.endif

.if ${MK_NS_CACHING} != "no"
FILES+= nscd
.endif

FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

.include <bsd.prog.mk>
