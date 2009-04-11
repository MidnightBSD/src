#!/bin/mksh
#
# $MidnightBSD: src/tools/man/makeman.sh,v 1.3 2009/04/11 20:49:30 laffer1 Exp $
#
# Copyright (C) 2009
#         Lucas Holt. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# usage:
# makeman.sh /usr/share/man/ outputdir

DIR=$1
OUT=$2
MANPATHS="man1 man2 man3 man4 man5 man6 man7 man8 man9"
TIDY=`which tidy`

if [ -d "$DIR" ]; then
	echo "Using $DIR for man files"
else
	echo "Bad directory"
	exit 1
fi

if [ -d "$OUT" ]; then
	echo "Using $OUT for output"
else
	echo "Bad output directory"
	exit 1
fi

# create initial index file
cp head.inc.html "$OUT"/index.html
echo "<h2>Sections</h2><ul>" >> "$OUT"/index.html
sed -i -e 's/%%TITLE%%/Online Manual Pages/g' "$OUT/index.html"

# interate through man paths /usr/share/man/man1, man2, ..., manN
for manpath in $MANPATHS; do
	if [ -d "$DIR/$manpath" ]; then
		# add section to index
		echo "<li><a href='$manpath.html'>$manpath</a></li>" >> "$OUT"/index.html

		# create section page
		cp head.inc.html "$OUT/$manpath".html
		echo "<ul>" >> "$OUT/$manpath".html

		# generate man pages as html
		for file in $DIR/$manpath/*.gz ; do
			echo "$file"
			MYNAME=`basename  "$file" .gz`
			if [ -f "$file" ]; then
				gunzip -q -c "$file" | groff -Thtml -m man  | $TIDY -q -asxhtml > "$OUT/$MYNAME".html
				echo "<li><a href='$MYNAME.html'>$MYNAME</a></li>" >> "$OUT/$manpath".html
			fi
		done
		echo "</ul>" >> "$OUT/$manpath".html

		# append footer for section document.
		cat foot.inc.html >> "$OUT/$manpath".html
		sed -i -e 's/%%TITLE%%/Online Manual Pages/g' "$OUT/$manpath.html"
	else
		echo "$DIR/$manpath does not exist"
	fi
done

# finish index document
echo "</ul>" >> "$OUT"/index.html
cat foot.inc.html >> "$OUT"/index.html
