# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/variables/opt_V/Makefile 236347 2012-05-31 01:07:52Z obrien $
#
# Test the -V option
#

FOO=	foo
FOOBAR=	${FOO}bar

test1:
	@echo "-V FOOBAR"
	@${MAKE} -V FOOBAR

test2:
	@echo '-V "$${FOOBAR}"'
	@${MAKE} -V '$${FOOBAR}'
