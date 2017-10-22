#!/bin/sh
#
# $FreeBSD: release/7.0.0/usr.bin/less/zless.sh 146314 2005-05-17 11:14:11Z des $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"
