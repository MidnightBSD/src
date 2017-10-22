#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/geom_raid3/conf.sh 153187 2005-12-07 01:28:59Z pjd $

name="test"
class="raid3"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
