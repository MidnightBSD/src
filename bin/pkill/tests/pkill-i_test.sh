#!/bin/sh
# $FreeBSD: stable/11/bin/pkill/tests/pkill-i_test.sh 263351 2014-03-19 12:46:04Z jmmv $

base=`basename $0`

echo "1..1"

name="pkill -i"
sleep=$(pwd)/sleep.txt
usleep="${sleep}XXX"
touch $usleep
lsleep="${sleep}xxx"
ln -sf /bin/sleep $usleep
$usleep 5 &
sleep 0.3
pkill -f -i $lsleep
ec=$?
case $ec in
0)
	echo "ok - $name"
	;;
*)
	echo "not ok - $name"
	;;
esac
rm -f $sleep $usleep
