# $FreeBSD: release/10.0.0/tools/build/mk/Makefile.boot 142640 2005-02-27 11:22:58Z ru $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib
