# $FreeBSD: release/10.0.0/tools/regression/usr.bin/join/regress.sh 137587 2004-11-11 19:47:55Z nik $

echo 1..1

REGRESSION_START($1)

REGRESSION_TEST_ONE(`join -t , -a1 -a2 -e "(unknown)" -o 0,1.2,2.2 regress.1.in regress.2.in')

REGRESSION_END()
