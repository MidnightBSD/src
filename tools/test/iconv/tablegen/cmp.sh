#!/bin/sh
# $FreeBSD: stable/11/tools/test/iconv/tablegen/cmp.sh 247414 2013-02-27 19:59:58Z jmg $

diff -I\$\FreeBSD $1 $2 | grep '^-' >/dev/null && printf "\tDIFFER: $1 $2\n" && exit 0 || exit 0
