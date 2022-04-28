#!/bin/sh
#

:>keywords
:>rcsid
git ls-files | \
while read f ; do
	egrep -l '^(#|\.\\"|/\*)[[:space:]]+\$MidnightBSD[:\$]' $f >>keywords
	egrep -l '__RCSID\("\$MidnightBSD[:\$]' $f >>rcsid
done
sort -u keywords rcsid | xargs perl -n -i -e '
	$strip = $ARGV if /\$(Id|OpenBSD):.*\$/;
	print unless (($strip eq $ARGV || /__RCSID/) && /\$MidnightBSD[:\$]/);
'
