# $MidnightBSD$
SRCS=	parse.y doas.c env.c execvpe.c

PROG=	doas
MAN=	doas.1 doas.conf.5

BINOWN= root
BINMODE=4555

CFLAGS+= -I${.CURDIR}
CFLAGS+= -Wall

CFLAGS+=-DUSE_PAM -DDOAS_CONF=\"/etc/doas.conf\"
LDFLAGS+=-lpam
CFLAGS+=-DHAVE_LOGIN_CAP_H
LDFLAGS+=-lutil

WARNS=	0

.include <bsd.prog.mk>
