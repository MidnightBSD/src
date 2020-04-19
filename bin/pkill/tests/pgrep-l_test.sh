#!/bin/sh
# $FreeBSD: stable/11/bin/pkill/tests/pgrep-l_test.sh 263351 2014-03-19 12:46:04Z jmmv $

base=`basename $0`

echo "1..1"

name="pgrep -l"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pid=$!
if [ "$pid $sleep 5" = "`pgrep -f -l $sleep`" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
fi
kill $pid
rm -f $sleep
