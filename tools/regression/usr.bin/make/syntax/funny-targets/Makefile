# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/syntax/funny-targets/Makefile 238143 2012-07-05 18:23:36Z obrien $
#
# PR bin/6612 / GRN r102178
#

test1: colons::colons::target

colons::colons::target:
	@echo '${.TARGET}'

test2: exclamation!target

exclamation!target:
	@echo '${.TARGET}
