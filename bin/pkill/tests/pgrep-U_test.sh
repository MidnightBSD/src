#!/bin/sh
# $FreeBSD: stable/11/bin/pkill/tests/pgrep-U_test.sh 263351 2014-03-19 12:46:04Z jmmv $

base=`basename $0`

echo "1..2"

name="pgrep -U <uid>"
ruid=`id -ur`
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -U $ruid $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
fi
kill $chpid
rm -f $sleep

name="pgrep -U <user>"
ruid=`id -urn`
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -U $ruid $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
fi
kill $chpid
rm -f $sleep
