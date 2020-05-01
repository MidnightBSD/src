#
# From: Id: Makefile,v 1.2 2004/01/15 16:20:56 pkern Exp
#
# $FreeBSD: stable/11/usr.sbin/ipfwpcap/Makefile 319279 2017-05-31 05:20:59Z delphij $
#

PROG=	ipfwpcap

LIBADD=	pcap
CFLAGS+=-I${.CURDIR}/../../contrib/libpcap

MAN=	ipfwpcap.8

.include <bsd.prog.mk>

test:	$(CMD)
	-rm /var/run/ipfwpcap.2000.pid
	./ipfwpcap -d 2000 - | tcpdump -r - -n -s 2000 -X

