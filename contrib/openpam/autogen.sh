#!/bin/sh
#
# $Id: autogen.sh,v 1.1.1.2 2012-07-21 14:57:34 laffer1 Exp $
#

aclocal
libtoolize --copy --force
autoheader
automake -a -c --foreign
autoconf
