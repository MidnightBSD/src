# $MidnightBSD: src/usr.bin/sudo/Makefile.inc,v 1.2 2011/01/20 22:17:41 laffer1 Exp $

LIBSUDO=	${.OBJDIR}/../lib/libsudo.a

CFLAGS+=-DSUDOERS_UID=0 -DSUDOERS_GID=0 -DSUDOERS_MODE=0440 -D_BSD_SOURCE -D_PATH_SUDOERS=\"/etc/sudoers\"

WARNS?=	0

.include "../Makefile.inc"
