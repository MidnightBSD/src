# $FreeBSD: stable/11/usr.bin/uuencode/tests/regress.sh 263227 2014-03-16 08:04:06Z jmmv $

echo 1..2

REGRESSION_START($1)

REGRESSION_TEST(`traditional', `uuencode regress.in <${SRCDIR}/regress.in')
REGRESSION_TEST(`base64', `uuencode -m regress.in <${SRCDIR}/regress.in')

REGRESSION_END()
