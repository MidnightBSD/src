#!/bin/sh
# $FreeBSD: stable/11/tools/regression/atm/proto_sscfu/RunTest.sh 125204 2004-01-29 16:01:57Z harti $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscfu

$LOCALBASE/bin/ats_sscfu $options $DATA/Funcs $DATA/EST* $DATA/REL* \
$DATA/REC* $DATA/RES* $DATA/DATA* $DATA/UDATA*
