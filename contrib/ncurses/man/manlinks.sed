# $Id: manlinks.sed,v 1.1.1.1 2006-02-25 02:26:09 laffer1 Exp $
##############################################################################
# Copyright (c) 2000 Free Software Foundation, Inc.                          #
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
# Given a manpage (nroff) as input, writes a list of the names that are
# listed in the "NAME" section, i.e., the names that we would like to use
# as aliases for the manpage -T.Dickey
/^'\\"/d
/\.\\"/d
/^\.br/d
/^\.sp/d
s/\\f.//g
s/[:,]/ /g
s/^[ 	][ 	]*//
s/[ 	][ 	]*$//
s/[ 	][ 	]*/ /g
s/\.SH[ 	][ 	]*/.SH_(/
#
/^\.SH_(NAME/,/^\.SH_(SYNOPSIS/{
s/\\-.*/ -/
/ -/{
	s/ -.*//
	s/ /\
/g
}
/^-/{
	d
}
s/ /\
/g
}
/^\.SH_(SYNOPSIS/,/^\.SH_(DESCRIPTION/{
	/^#/d
	/^[^(]*$/d
	s/^\([^ (]* [^ (]* [*]*\)//g
	s/^\([^ (]* [*]*\)//g
	s/\.SH_(/.SH_/
	s/(.*//
	s/\.SH_/.SH_(/
}
/^\.SH_(DESCRIPTION/,${
	d
}
/^\./d
