#!/bin/sh
# $FreeBSD: stable/11/tools/regression/atm/proto_sscop/RunTest.sh 125204 2004-01-29 16:01:57Z harti $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscop

$LOCALBASE/bin/ats_sscop $options $DATA/Funcs $DATA/S*
