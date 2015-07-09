dnl $FreeBSD: stable/10/usr.bin/m4/tests/args2.m4 234852 2012-04-30 22:00:34Z bapt $
dnl $OpenBSD: src/regress/usr.bin/m4/args2.m4,v 1.1 2008/08/16 09:57:12 espie Exp $
dnl Preserving spaces within nested parentheses
define(`foo',`$1')dnl
foo((	  check for embedded spaces))
