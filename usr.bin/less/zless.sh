#!/bin/sh
#
# $MidnightBSD$

export LESSOPEN="||/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"
