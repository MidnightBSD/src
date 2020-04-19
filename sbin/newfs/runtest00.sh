#!/bin/sh
# $FreeBSD: stable/11/sbin/newfs/runtest00.sh 92722 2002-03-19 21:05:29Z phk $

set -e

MD=99
(
for s in 1m 4m 60m 120m 240m 1g
do
	(
	mdconfig -d -u $MD || true
	mdconfig -a -t malloc -s $s -u $MD
	disklabel -r -w md$MD auto
	./newfs -R /dev/md${MD}c
	) 1>&2
	md5 < /dev/md${MD}c
done
mdconfig -d -u $MD 1>&2 || true
) 
