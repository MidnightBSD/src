#!/bin/sh
#
# $FreeBSD: release/10.0.0/tools/regression/geom_uzip/test-1.sh 150734 2005-09-29 22:41:20Z fjoe $
#

mntpoint="/mnt/test-1"

#
# prepare
kldload geom_uzip
uudecode test-1.img.uzip.uue
num=`mdconfig -an -f test-1.img.uzip` || exit 1
sleep 1

#
# mount
mkdir -p "${mntpoint}"
mount -o ro /dev/md${num}.uzip "${mntpoint}" || exit 1

#
# compare
#cat "${mntpoint}/etalon.txt"
diff -u etalon/etalon.txt "${mntpoint}/etalon.txt"
if [ $? -eq 0 ]; then
	echo "PASS"
else
	echo "FAIL"
fi

#
# cleanup
umount "${mntpoint}"
rmdir "${mntpoint}"
mdconfig -d -u ${num}
sleep 1
kldunload geom_uzip
