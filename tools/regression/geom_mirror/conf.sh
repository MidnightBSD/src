#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/geom_mirror/conf.sh 153186 2005-12-07 01:27:23Z pjd $

name="test"
class="mirror"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
