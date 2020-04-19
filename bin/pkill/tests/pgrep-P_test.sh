#!/bin/sh
# $FreeBSD: stable/11/bin/pkill/tests/pgrep-P_test.sh 263351 2014-03-19 12:46:04Z jmmv $

base=`basename $0`

echo "1..1"

name="pgrep -P <ppid>"
ppid=$$
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -P $ppid $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
fi
kill $chpid
rm -f $sleep
