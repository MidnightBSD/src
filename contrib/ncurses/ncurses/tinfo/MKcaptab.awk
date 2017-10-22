#!/bin/sh
##############################################################################
# Copyright (c) 1998-2000,2006 Free Software Foundation, Inc.                #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, distribute    #
# with modifications, sublicense, and/or sell copies of the Software, and to #
# permit persons to whom the Software is furnished to do so, subject to the  #
# following conditions:                                                      #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
# Except as contained in this notice, the name(s) of the above copyright     #
# holders shall not be used in advertising or otherwise to promote the sale, #
# use or other dealings in this Software without prior written               #
# authorization.                                                             #
##############################################################################
# $Id: MKcaptab.awk,v 1.13 2006/04/22 21:46:17 tom Exp $
AWK=${1-awk}
DATA=${2-../include/Caps}

cat <<'EOF'
/*
 *	comp_captab.c -- The names of the capabilities indexed via a hash
 *		         table for the compiler.
 *
 */

#include <ncurses_cfg.h>
#include <curses.priv.h>
#include <tic.h>
#include <term.h>

EOF

./make_hash 1 info <$DATA
./make_hash 3 cap  <$DATA

cat <<'EOF'
const struct alias _nc_capalias_table[] =
{
EOF

$AWK <$DATA '
$1 == "capalias"	{
		    if ($3 == "IGNORE")
			to = "(char *)NULL";
		    else
			to = "\"" $3 "\"";
		    printf "\t{\"%s\", %s, \"%s\"},\t /* %s */\n",
				$2, to, $4, $5
		}
'

cat <<'EOF'
	{(char *)NULL, (char *)NULL, (char *)NULL}
};

const struct alias _nc_infoalias_table[] =
{
EOF

$AWK <$DATA '
$1 == "infoalias"	{
		    if ($3 == "IGNORE")
			to = "(char *)NULL";
		    else
			to = "\"" $3 "\"";
		    printf "\t{\"%s\", %s, \"%s\"},\t /* %s */\n",
				$2, to, $4, $5
		}
'

cat <<'EOF'
	{(char *)NULL, (char *)NULL, (char *)NULL}
};

NCURSES_EXPORT(const struct name_table_entry *) _nc_get_table (bool termcap)
{
	return termcap ? _nc_cap_table: _nc_info_table ;
}

NCURSES_EXPORT(const struct name_table_entry * const *) _nc_get_hash_table (bool termcap)
{
	return termcap ? _nc_cap_hash_table: _nc_info_hash_table ;
}
EOF
