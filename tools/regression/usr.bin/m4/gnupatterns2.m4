dnl $FreeBSD: release/10.0.0/tools/regression/usr.bin/m4/gnupatterns2.m4 234852 2012-04-30 22:00:34Z bapt $
define(`zoinx',dnl
`patsubst($1,\(\w+\)\(\W*\),\1 )')dnl
zoinx(acosl asinl atanl \
       cosl sinl tanl \
       coshl sinhl tanhl)
