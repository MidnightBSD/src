#!/bin/sh
# $FreeBSD: stable/11/gnu/usr.bin/groff/mdate.sh 307631 2016-10-19 13:19:45Z emaste $

set -e
test -r "$1"
export LC_ALL=C
changelog_date=$(sed -E -n '1s/^([0-9]{4}-[0-9]{2}-[0-9]{2}).*$/\1/p' "$1")
echo $(date -j -f %Y-%m-%d +"%e %B %Y" $changelog_date)
