#
# $FreeBSD: release/10.0.0/tools/tools/ether_reflect/Makefile 219524 2011-03-11 19:19:47Z brucec $
#
# A Makefile that builds both the ether_reflect program and its manual page.

PROG=   ether_reflect
DPADD=	${LIBPCAP}
LDADD=	-lpcap
CFLAGS+=-DHAVE_SNPRINTF

.include <bsd.prog.mk>
