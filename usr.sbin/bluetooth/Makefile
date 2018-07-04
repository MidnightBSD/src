# $MidnightBSD$
# $Id: Makefile,v 1.5 2003/09/08 02:28:35 max Exp $
# $FreeBSD: stable/10/usr.sbin/bluetooth/Makefile 282835 2015-05-13 10:07:50Z ngie $

.include <bsd.own.mk>

SUBDIR= \
	bt3cfw \
	btpand \
	hccontrol \
	hcsecd \
	hcseriald \
	l2control \
	l2ping \
	rfcomm_pppd \
	sdpcontrol \
	sdpd

.if ${MK_USB} != "no"
SUBDIR+=	ath3kfw
SUBDIR+=	bcmfw
SUBDIR+=	bthidcontrol
SUBDIR+=	bthidd
.endif

.include <bsd.subdir.mk>

