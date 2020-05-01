# $FreeBSD: stable/11/usr.sbin/praudit/tests/Makefile 335290 2018-06-17 17:31:16Z asomers $

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/usr.sbin/praudit

ATF_TESTS_SH=	praudit_test

${PACKAGE}FILES+=			\
		input/trail 		\
		input/corrupted		\
		input/del_comma 	\
		input/del_underscore 	\
		input/no_args 		\
		input/numeric_form 	\
		input/raw_form 		\
		input/same_line 	\
		input/short_form 	\
		input/xml_form

TEST_METADATA+= timeout="10"

.include <bsd.test.mk>
