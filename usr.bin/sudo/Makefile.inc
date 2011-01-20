# $MidnightBSD: src/usr.bin/sudo/Makefile.inc,v 1.1 2007/02/07 02:52:23 laffer1 Exp $

LIBSUDO=	${.OBJDIR}/../lib/libsudo.a

CFLAGS+=-DSUDOERS_UID=0 -DSUDOERS_GID=0 -DSUDOERS_MODE=0440

.include "../Makefile.inc"
