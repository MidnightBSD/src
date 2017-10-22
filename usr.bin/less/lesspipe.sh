#! /bin/sh
# ex:ts=8

# $FreeBSD: release/7.0.0/usr.bin/less/lesspipe.sh 169956 2007-05-24 18:28:08Z le $

case "$1" in
	*.Z)
		exec uncompress -c "$1"	2>/dev/null
		;;
	*.gz)
		exec gzip -d -c "$1"	2>/dev/null
		;;
	*.bz2)
		exec bzip2 -d -c "$1"	2>/dev/null
		;;
esac
