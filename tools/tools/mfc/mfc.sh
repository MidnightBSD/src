#!/bin/sh
#
# Merge an entire directory from HEAD to a given branch
#
# $FreeBSD: release/7.0.0/tools/tools/mfc/mfc.sh 135743 2004-09-24 15:24:12Z des $
#

if [ $# -eq 1 -a -f CVS/Tag ] ; then
	set -- "${1}" "$(sed -e 's/^T//' <CVS/Tag)"
	echo "Assuming branch is $2"
fi

if [ $# -ne 2 ] ; then
	echo "usage: mfc <dir> <branch>" 1>&2
	exit 1
fi

dir="${1}"
branch="${2}"

set -e

# Get sources from HEAD
/usr/bin/cvs -f -Q up -kk -Pd -A "${dir}"

# Fake a cvs add...
find "${dir}" -type d -name CVS |
while read d ; do
	echo "${d%/CVS}"

	# Fix up Entries so the files look newly added
	sed -E -i'' -e \
	    "s|^/([^/]+)/.*|/\1/0/Merged \1 from HEAD//T${branch}|" \
	    "${d}/Entries"

	# Set directory tag
	echo "T${branch}" >"$d/Tag"
done
