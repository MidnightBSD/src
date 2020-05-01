#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.sbin/rmt/Makefile 318513 2017-05-19 02:12:10Z rgrimes $

PROG=	rmt
MAN=	rmt.8

# called from /usr/src/etc/Makefile
etc-rmt:
	rm -f ${DESTDIR}/etc/rmt
	ln -s ..${BINDIR}/rmt ${DESTDIR}/etc/rmt

.include <bsd.prog.mk>
