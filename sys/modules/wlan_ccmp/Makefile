# $FreeBSD: stable/11/sys/modules/wlan_ccmp/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/net80211
.PATH: ${SRCTOP}/sys/crypto/rijndael

KMOD=	wlan_ccmp
SRCS=	ieee80211_crypto_ccmp.c
SRCS+=	rijndael-alg-fst.c rijndael-api.c
SRCS+=	opt_wlan.h

.include <bsd.kmod.mk>
