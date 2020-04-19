#!/bin/sh
# $FreeBSD: stable/11/bin/pkill/tests/pgrep-i_test.sh 263351 2014-03-19 12:46:04Z jmmv $

base=`basename $0`

echo "1..1"

name="pgrep -i"
sleep=$(pwd)/sleep.txt
usleep="${sleep}XXX"
touch $usleep
lsleep="${sleep}xxx"
ln -sf /bin/sleep $usleep
$usleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -i $lsleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
fi
kill $chpid
rm -f $sleep $usleep
