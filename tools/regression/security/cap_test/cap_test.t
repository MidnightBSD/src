#!/bin/sh
#
# $FreeBSD: release/10.0.0/tools/regression/security/cap_test/cap_test.t 224653 2011-08-04 17:17:57Z jonathan $
#

if test -z "${DIR}" ; then DIR=$( make -V .OBJDIR ); fi
if test -z "${DIR}" ; then DIR=$( dirname $0 ); fi

make > /dev/null || exit 1
$DIR/cap_test $*

