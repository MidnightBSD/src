#!/bin/sh
# $FreeBSD: stable/11/tools/test/sort/regression/cmp.sh 235274 2012-05-11 16:04:55Z gabor $

diff $1 $2 | grep '^-' >/dev/null && echo DIFFER: $1 $2 && exit 0 || exit 0
