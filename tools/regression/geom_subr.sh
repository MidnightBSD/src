#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/geom_subr.sh 153184 2005-12-07 01:20:18Z pjd $

kldstat -q -m g_${class} || g${class} load || exit 1

devwait()
{
	while :; do
		if [ -c /dev/${class}/${name} ]; then
			return
		fi
		sleep 0.2
	done
}
