dnl $FreeBSD: stable/10/usr.bin/m4/tests/gnupatterns.m4 234852 2012-04-30 22:00:34Z bapt $
patsubst(`string with a + to replace with a minus', `+', `minus')
patsubst(`string with aaaaa to replace with a b', `a+', `b')
patsubst(`+string with a starting + to replace with a minus', `^+', `minus')
