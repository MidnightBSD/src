# $FreeBSD: stable/11/usr.sbin/wpa/wpa_priv/Makefile 319190 2017-05-30 04:45:11Z ngie $

.include "../Makefile.inc"

.PATH.c:${WPA_SUPPLICANT_DISTDIR} \
	${WPA_DISTDIR}/src/drivers

PROG=	wpa_priv
SRCS=	drivers.c os_unix.c eloop.c common.c wpa_debug.c wpabuf.c wpa_priv.c \
	driver_common.c l2_packet_freebsd.c

LIBADD=	pcap

.include "../Makefile.crypto"

.include <bsd.prog.mk>
