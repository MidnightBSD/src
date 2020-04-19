#!/bin/sh
# $FreeBSD: stable/11/bin/pkill/tests/pkill-P_test.sh 263351 2014-03-19 12:46:04Z jmmv $

base=`basename $0`

echo "1..1"

name="pkill -P <ppid>"
ppid=$$
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -P $ppid $sleep
ec=$?
case $ec in
0)
	echo "ok - $name"
	;;
*)
	echo "not ok - $name"
	;;
esac

rm -f $sleep
