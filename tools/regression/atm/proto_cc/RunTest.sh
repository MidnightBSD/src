#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/atm/proto_cc/RunTest.sh 133639 2004-08-13 09:27:21Z harti $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_cc

$LOCALBASE/bin/ats_cc $options $DATA/CC_Funcs $DATA/CC_??_??
