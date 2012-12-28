#!/bin/sh
# $MidnightBSD$

set -m
/root/hast/sbin/hastd/ucarp_up.sh &
set +m
exit 0
