dnl $FreeBSD: release/10.0.0/tools/regression/usr.bin/m4/includes.m4 234852 2012-04-30 22:00:34Z bapt $
dnl $OpenBSD: src/regress/usr.bin/m4/includes.m4,v 1.1 2008/08/16 10:02:32 espie Exp $
dnl Check that include can occur within parameters
define(`foo', include(includes.aux))dnl
foo
