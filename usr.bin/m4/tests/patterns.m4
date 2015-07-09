dnl $FreeBSD: stable/10/usr.bin/m4/tests/patterns.m4 234852 2012-04-30 22:00:34Z bapt $
dnl $OpenBSD: src/regress/usr.bin/m4/patterns.m4,v 1.4 2003/06/08 20:11:45 espie Exp $
patsubst(`quote s in string', `(s)', `\\\1')
patsubst(`check whether subst
over several lines
works as expected', `^', `>>>')
patsubst(`# This is a line to zap
# and a second line
keep this one', `^ *#.*
')
dnl Special case: empty regexp
patsubst(`empty regexp',`',`a ')
