#!/bin/sh
#
# $MidnightBSD: src/usr.bin/less/zless.sh,v 1.2 2007/04/10 22:05:46 laffer1 Exp $

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"
