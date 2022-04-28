#!/bin/sh

:>keywords
:>rcsid
svn list -R | grep -v '/$' | \
while read f ; do
	svn proplist -v $f | grep -q 'MidnightBSD=%H' || continue
	egrep -l '^(#|\.\\"|/\*)[[:space:]]+\$MidnightBSD[:\$]' $f >>keywords
	egrep -l '__RCSID\("\$MidnightBSD[:\$]' $f >>rcsid
done
sort -u keywords rcsid | xargs perl -n -i -e '
	$strip = $ARGV if /\$(Id|OpenBSD):.*\$/;
	print unless (($strip eq $ARGV || /__RCSID/) && /\$MidnightBSD[:\$]/);
'
