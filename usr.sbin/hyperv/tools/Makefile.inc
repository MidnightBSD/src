# $FreeBSD: stable/11/usr.sbin/hyperv/tools/Makefile.inc 311353 2017-01-05 03:20:00Z sephe $

CFLAGS.gcc+= -Wno-uninitialized
.include "../../Makefile.inc"
