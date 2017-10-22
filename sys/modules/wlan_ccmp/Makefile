# $FreeBSD: release/7.0.0/sys/modules/wlan_ccmp/Makefile 138905 2004-12-16 01:10:48Z sam $

.PATH: ${.CURDIR}/../../net80211
.PATH: ${.CURDIR}/../../crypto/rijndael

KMOD=	wlan_ccmp
SRCS=	ieee80211_crypto_ccmp.c
SRCS+=	rijndael-alg-fst.c rijndael-api.c

.include <bsd.kmod.mk>
