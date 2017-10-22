# $FreeBSD: release/10.0.0/tools/regression/usr.bin/uudecode/regress.sh 137587 2004-11-11 19:47:55Z nik $

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST_ONE(`uudecode -p < regress.traditional.in', `traditional')
REGRESSION_TEST_ONE(`uudecode -p < regress.base64.in', `base64')

REGRESSION_END()
