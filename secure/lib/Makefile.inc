# $FreeBSD: stable/11/secure/lib/Makefile.inc 319189 2017-05-30 04:42:36Z ngie $

.include "../Makefile.inc"
.if exists(${.CURDIR:H:H:H}/lib/Makefile.inc)
.include "${.CURDIR:H:H:H}/lib/Makefile.inc"
.endif
