#	@(#)Makefile	8.5 (Berkeley) 3/31/94
# $FreeBSD: src/sbin/Makefile,v 1.168 2007/07/24 15:35:00 scottl Exp $
# $MidnightBSD: src/sbin/Makefile,v 1.11 2012/04/11 01:06:11 laffer1 Exp $

.include <bsd.own.mk>

# XXX MISSING:		icheck ncheck

SUBDIR=	adjkerntz \
	atacontrol \
	badsect \
	camcontrol \
	ccdconfig \
	clri \
	comcontrol \
	conscontrol \
	ddb \
	devfs \
	dhclient \
	dmesg \
	dump \
	dumpfs \
	dumpon \
	ffsinfo \
	fsck \
	fsck_ffs \
	fsck_msdosfs \
	fsdb \
	fsirand \
	gbde \
	geom \
	ggate \
	growfs \
	gvinum \
	hastctl \
	hastd \
	ifconfig \
	init \
	iscontrol \
	kldconfig \
	kldload \
	kldstat \
	kldunload \
	ldconfig \
	md5 \
	mdconfig \
	mdmfs \
	mknod \
	mksnap_ffs \
	mount \
	mount_cd9660 \
	mount_msdosfs \
	mount_nfs \
	mount_ntfs \
	mount_nullfs \
	mount_udf \
	mount_unionfs \
	newfs \
	newfs_msdos \
	nfsiod \
	nos-tun \
	ping \
	rcorder \
	reboot \
	recoverdisk \
	resolvconf \
	restore \
	route \
	savecore \
	setkey \
	shutdown \
	slattach \
	spppcontrol \
	startslip \
	swapon \
	sysctl \
	tunefs \
	umount \

.if ${MK_ATM} != "no"
SUBDIR+=	atm
.endif

.if ${MK_CXX} != "no"
SUBDIR+=	devd
.endif

.if ${MK_IPFILTER} != "no"
SUBDIR+=	ipf
.endif

.if ${MK_IPFW} != "no"
SUBDIR+=	ipfw
SUBDIR+=	natd
.endif

.if ${MK_PF} != "no"
SUBDIR+=	pfctl
SUBDIR+=	pflogd
.endif

.if ${MK_INET6} != "no"
SUBDIR+=	dhcp6c
SUBDIR+=	ping6
SUBDIR+=	rtsol
.endif

.if ${MK_QUOTAS} != "no"
SUBDIR+=	quotacheck
.endif

.if ${MK_ROUTED} != "no"
SUBDIR+=	routed
.endif

.include <bsd.arch.inc.mk>

SUBDIR:=	${SUBDIR:O}

.include <bsd.subdir.mk>
