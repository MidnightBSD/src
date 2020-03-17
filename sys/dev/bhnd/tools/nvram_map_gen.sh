#!/bin/sh

# Use C locale to ensure AWK string comparisons always produce
# a stable sort order.

# $FreeBSD: stable/11/sys/dev/bhnd/tools/nvram_map_gen.sh 299692 2016-05-13 18:56:56Z emaste $

BHND_TOOLDIR="$(dirname $0)/"

LC_ALL=C; export LC_ALL

"${AWK:-/usr/bin/awk}" -f "$BHND_TOOLDIR/nvram_map_gen.awk" $@
