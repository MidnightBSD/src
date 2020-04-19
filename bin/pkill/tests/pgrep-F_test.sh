#!/bin/sh
# $FreeBSD: stable/11/bin/pkill/tests/pgrep-F_test.sh 263351 2014-03-19 12:46:04Z jmmv $

base=`basename $0`

echo "1..1"

name="pgrep -F <pidfile>"
pidfile=$(pwd)/pidfile.txt
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
echo $chpid > $pidfile
pid=`pgrep -f -F $pidfile $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
fi
kill "$chpid"
rm -f $pidfile
rm -f $sleep
