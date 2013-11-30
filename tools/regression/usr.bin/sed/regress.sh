# $FreeBSD: src/tools/regression/usr.bin/sed/regress.sh,v 1.9 2004/11/11 19:47:55 nik Exp $

REGRESSION_START($1)

echo '1..9'

REGRESSION_TEST(`G', `sed G < regress.in')
REGRESSION_TEST(`P', `sed P < regress.in')
REGRESSION_TEST(`psl', `sed \$!g\;P\;D < regress.in')
REGRESSION_TEST(`bcb', `sed s/X/$(jot -n -bx -s "" 2043)\\\\zz/ < regress.in')
REGRESSION_TEST(`y', `echo -n foo | sed y/o/O/')
REGRESSION_TEST(`sg', `echo foo | sed s/,*/,/g')
REGRESSION_TEST(`s3', `echo foo | sed s/,*/,/3')
REGRESSION_TEST(`s4', `echo foo | sed s/,*/,/4')
REGRESSION_TEST(`s5', `echo foo | sed s/,*/,/5')

REGRESSION_END()
