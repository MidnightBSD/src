#! /bin/sh
# Test suite for exclude.
# Copyright (C) 2009, 2010 Free Software Foundation, Inc.
# This file is part of the GNUlib Library.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

TMP=excltmp.$$
LIST=flist.$$
ERR=0

# Test FNM_LEADING_DIR

cat > $LIST <<EOT
foo*
bar
Baz
EOT

cat > $TMP <<EOT
bar: 1
bar/qux: 1
barz: 0
foo/bar: 1
EOT

./test-exclude$EXEEXT -leading_dir $LIST -- bar bar/qux barz foo/bar |
 tr -d '\015' |
 diff -c $TMP - || ERR=1

rm -f $TMP $LIST
exit $ERR
