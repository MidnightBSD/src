#!/bin/sh
#
# $FreeBSD: release/10.0.0/tools/regression/geom_uzip/test-2.sh 150734 2005-09-29 22:41:20Z fjoe $
#

#
# prepare
kldload geom_uzip
uudecode test-1.img.uzip.uue
num=`mdconfig -an -f test-1.img.uzip` || exit 1
sleep 1

#
# destroy
kldunload geom_uzip
