#!/bin/sh
# $FreeBSD: stable/9/tools/regression/geom_stripe/conf.sh 153189 2005-12-07 01:30:44Z pjd $

name="test"
class="stripe"
base=`basename $0`

. `dirname $0`/../geom_subr.sh
