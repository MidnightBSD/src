# $FreeBSD: release/10.0.0/tools/regression/usr.bin/uuencode/regress.sh 137587 2004-11-11 19:47:55Z nik $

echo 1..2

REGRESSION_START($1)

# To make sure we end up with matching headers.
umask 022

REGRESSION_TEST(`traditional', `uuencode regress.in < regress.in')
REGRESSION_TEST(`base64', `uuencode -m regress.in < regress.in')

REGRESSION_END()
