#!/bin/sh
#

xargs perl -n -i -e '
	print;
	s/\$(Id|OpenBSD): [^\$]*/\$MidnightBSD/ && print;
' <keywords

xargs perl -n -i -e '
	print;
	m/^\#include "includes.h"/ && print "__RCSID(\"\$MidnightBSD\$\");\n";
' <rcsid
