#!/bin/sh
# $FreeBSD: stable/11/tools/regression/atm/proto_uni/RunTest.sh 125204 2004-01-29 16:01:57Z harti $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_uni

$LOCALBASE/bin/ats_sig $options $DATA/Funcs $DATA/L3MU_Funcs $DATA/Restart.??? \
	$DATA/Unknown.??? $DATA/Incoming.??? $DATA/MpOut.??? $DATA/MpIn.??? \
	$DATA/L???_??_??
