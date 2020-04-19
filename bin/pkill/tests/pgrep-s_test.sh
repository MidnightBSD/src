#!/bin/sh
# $FreeBSD: stable/11/bin/pkill/tests/pgrep-s_test.sh 263351 2014-03-19 12:46:04Z jmmv $

base=`basename $0`

echo "1..2"

name="pgrep -s <sid>"
sid=`ps -o tsid -p $$ | tail -1`
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -s $sid $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
fi
kill $chpid
rm -f $sleep

name="pgrep -s 0"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -s 0 $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
fi
kill $chpid
rm -f $sleep
