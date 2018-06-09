# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/hyperv/tools/Makefile.inc 310735 2016-12-29 05:32:34Z sephe $

CFLAGS.gcc+= -Wno-uninitialized
.include "../../Makefile.inc"
