#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/10.0.0/usr.bin/whois/Makefile 116229 2003-06-12 02:46:47Z mike $

PROG=	whois

.if defined(SOCKS)
CFLAGS+=-DSOCKS
CFLAGS+=-Dconnect=Rconnect -Dgetsockname=Rgetsockname -Dlisten=Rlisten \
	-Daccept=Raccept -Drcmd=Rrcmd -Dbind=Rbind -Dselect=Rselect
LDADD+=	-lsocks
.endif

.include <bsd.prog.mk>
