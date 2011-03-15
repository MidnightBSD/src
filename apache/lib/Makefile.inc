# $MidnightBSD: src/apache/lib/Makefile.inc,v 1.2 2010/08/14 21:41:56 laffer1 Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
