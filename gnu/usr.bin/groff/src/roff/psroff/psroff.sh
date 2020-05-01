#! /bin/sh -
#
# $FreeBSD: stable/11/gnu/usr.bin/groff/src/roff/psroff/psroff.sh 79552 2001-07-10 17:23:07Z ru $

exec groff -Tps -l -C ${1+"$@"}
