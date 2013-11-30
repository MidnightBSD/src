#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pgrep-l.t,v 1.1 2005/03/20 12:38:08 pjd Exp $

base=`basename $0`

echo "1..1"

name="pgrep -l"
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
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
