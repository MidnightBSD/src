#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/geom_gate/test-2.sh 128883 2004-05-03 18:29:54Z pjd $

base=`basename $0`
us=45
work=`mktemp /tmp/$base.XXXXXX` || exit 1
src=`mktemp /tmp/$base.XXXXXX` || exit 1

dd if=/dev/random of=$work bs=1m count=1 >/dev/null 2>&1
dd if=/dev/random of=$src bs=1m count=1 >/dev/null 2>&1
sum=`md5 -q $src`

ggatel create -u $us $work

dd if=${src} of=/dev/ggate${us} bs=1m count=1 >/dev/null 2>&1

if [ `md5 -q $work` != $sum ]; then
	echo "FAIL"
else
	if [ `cat /dev/ggate${us} | md5 -q` != $sum ]; then
		echo "FAIL"
	else
		echo "PASS"
	fi
fi

ggatel destroy -u $us
rm -f $work $src
