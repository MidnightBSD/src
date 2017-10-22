# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/suffixes/basic/Makefile 146822 2005-05-31 14:13:07Z harti $
#
# Check that a simple suffix rule is correctly processed.

test1:	TEST1.b
	diff -u TEST1.a TEST1.b

.SUFFIXES:

.SUFFIXES: .a .b

.a.b:
	cat ${.IMPSRC} >${.TARGET}
