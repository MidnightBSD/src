# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/syntax/semi/Makefile 151443 2005-10-18 07:28:09Z harti $
#
# Test handling of escaped newlines.
#

.ifmake test1

FOO=
BAR=bar
FOO != echo ${FOO} ; echo ${BAR}

test1:
	echo ${FOO}

.endif

.ifmake test2

FOO=foo
BAR=bar
FOO != echo ${FOO} ; echo ${BAR}

test2:
	echo ${FOO}

.endif
