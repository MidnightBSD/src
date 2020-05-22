# $FreeBSD: stable/11/usr.bin/uudecode/tests/regress.sh 263227 2014-03-16 08:04:06Z jmmv $

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST_ONE(`uudecode -p <${SRCDIR}/regress.traditional.in', `traditional')
REGRESSION_TEST_ONE(`uudecode -p <${SRCDIR}/regress.base64.in', `base64')

REGRESSION_END()
