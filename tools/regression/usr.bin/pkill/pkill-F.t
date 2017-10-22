#!/bin/sh
# $FreeBSD: stable/9/tools/regression/usr.bin/pkill/pkill-F.t 149472 2005-08-25 20:11:39Z pjd $

base=`basename $0`

echo "1..1"

name="pkill -F <pidfile>"
pidfile=`mktemp /tmp/$base.XXXXXX` || exit 1
sleep=`mktemp /tmp/$base.XXXXXX` || exit 1
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
echo $! > $pidfile
pkill -f -F $pidfile $sleep
ec=$?
case $ec in
0)
	echo "ok - $name"
	;;
*)
	echo "not ok - $name"
	;;
esac

rm -f $pidfile
rm -f $sleep
