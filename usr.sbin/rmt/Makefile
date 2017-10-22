#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/10.0.0/usr.sbin/rmt/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	rmt
MAN=	rmt.8

# called from /usr/src/etc/Makefile
etc-rmt:
	rm -f ${DESTDIR}/etc/rmt
	ln -s ${BINDIR}/rmt ${DESTDIR}/etc/rmt

.include <bsd.prog.mk>
