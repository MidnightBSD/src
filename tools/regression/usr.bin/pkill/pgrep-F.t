#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/usr.bin/pkill/pgrep-F.t 149472 2005-08-25 20:11:39Z pjd $

base=`basename $0`

echo "1..1"

name="pgrep -F <pidfile>"
pidfile=`mktemp /tmp/$base.XXXXXX` || exit 1
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
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
