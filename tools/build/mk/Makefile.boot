# $FreeBSD: stable/11/tools/build/mk/Makefile.boot 298881 2016-05-01 16:20:14Z pfg $

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

# we do not want to capture dependencies referring to the above
UPDATE_DEPENDFILE= no
