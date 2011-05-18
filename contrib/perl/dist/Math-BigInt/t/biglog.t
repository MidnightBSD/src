#!/usr/bin/perl -w

# Test blog function (and bpow, since it uses blog), as well as bexp().

# It is too slow to be simple included in bigfltpm.inc, where it would get
# executed 3 times. One time would be under BareCalc, which shouldn't make any
# difference since there is no CALC->_log() function, and one time under a
# subclass, which *should* work.

# But it is better to test the numerical functionality, instead of not testing
# it at all (which did lead to wrong answers for 0 < $x < 1 in blog() in
# versions up to v1.63, and for bsqrt($x) when $x << 1 for instance).

use strict;
use Test::More tests => 70;

use Math::BigFloat;
use Math::BigInt;

my $cl = "Math::BigInt";

#############################################################################
# test log($n) in BigInt (broken until 1.80)

is ($cl->new(2)->blog(), '0', "blog(2)");
is ($cl->new(288)->blog(), '5',"blog(288)");
is ($cl->new(2000)->blog(), '7', "blog(2000)");

#############################################################################
# test exp($n) in BigInt

is ($cl->new(1)->bexp(), '2', "bexp(1)");
is ($cl->new(2)->bexp(), '7',"bexp(2)");
is ($cl->new(3)->bexp(), '20', "bexp(3)");

#############################################################################
#############################################################################
# BigFloat tests

#############################################################################
# test log(2, N) where N > 67 (broken until 1.82)

$cl = "Math::BigFloat";

# These tests can take quite a while, but are nec. Maybe protect them with
# some alarm()?

# this triggers the calculation and caching of ln(2):
ok ($cl->new(5)->blog(undef,71), 
'1.6094379124341003746007593332261876395256013542685177219126478914741790');

# if the cache was correct, we should get this result, fast:
ok ($cl->new(2)->blog(undef,71), 
'0.69314718055994530941723212145817656807550013436025525412068000949339362');

ok ($cl->new(10)->blog(undef,71), 
'2.3025850929940456840179914546843642076011014886287729760333279009675726');

ok ($cl->new(21)->blog(undef,71), 
'3.0445224377234229965005979803657054342845752874046106401940844835750742');

#############################################################################

# These tests are now really fast, since they collapse to blog(10), basically
# Don't attempt to run them with older versions. You are warned.

# $x < 0 => NaN
ok ($cl->new(-2)->blog(), 'NaN');
ok ($cl->new(-1)->blog(), 'NaN');
ok ($cl->new(-10)->blog(), 'NaN');
ok ($cl->new(-2,2)->blog(), 'NaN');

my $ten = $cl->new(10)->blog();

# 10 is cached (up to 75 digits)
ok ($cl->new(10)->blog(), '2.302585092994045684017991454684364207601');

# 0.1 is using the cached value for log(10), too

ok ($cl->new(0.1)->blog(), -$ten);
ok ($cl->new(0.01)->blog(), -$ten * 2);
ok ($cl->new(0.001)->blog(), -$ten * 3);
ok ($cl->new(0.0001)->blog(), -$ten * 4);

# also cached
ok ($cl->new(2)->blog(), '0.6931471805599453094172321214581765680755');
ok ($cl->new(4)->blog(), $cl->new(2)->blog * 2);

# These are still slow, so do them only to 10 digits

ok ($cl->new('0.2')->blog(undef,10), '-1.609437912');
ok ($cl->new('0.3')->blog(undef,10), '-1.203972804');
ok ($cl->new('0.4')->blog(undef,10), '-0.9162907319');
ok ($cl->new('0.5')->blog(undef,10), '-0.6931471806');
ok ($cl->new('0.6')->blog(undef,10), '-0.5108256238');
ok ($cl->new('0.7')->blog(undef,10), '-0.3566749439');
ok ($cl->new('0.8')->blog(undef,10), '-0.2231435513');
ok ($cl->new('0.9')->blog(undef,10), '-0.1053605157');

ok ($cl->new('9')->blog(undef,10), '2.197224577');

ok ($cl->new('10')->blog(10,10),   '1.000000000');
ok ($cl->new('20')->blog(20,10),   '1.000000000');
ok ($cl->new('100')->blog(100,10), '1.000000000');

ok ($cl->new('100')->blog(10,10),  '2.000000000');	# 10 ** 2 == 100
ok ($cl->new('400')->blog(20,10),  '2.000000000');	# 20 ** 2 == 400

ok ($cl->new('4')->blog(2,10),  '2.000000000');		# 2 ** 2 == 4
ok ($cl->new('16')->blog(2,10), '4.000000000');		# 2 ** 4 == 16

ok ($cl->new('1.2')->bpow('0.3',10),  '1.056219968');	
ok ($cl->new('10')->bpow('0.6',10),   '3.981071706');

# blog should handle bigint input
is (Math::BigFloat::blog(Math::BigInt->new(100),10), 2, "blog(100)");

#############################################################################
# some integer results
is ($cl->new(2)->bpow(32)->blog(2),  '32', "2 ** 32");
is ($cl->new(3)->bpow(32)->blog(3),  '32', "3 ** 32");
is ($cl->new(2)->bpow(65)->blog(2),  '65', "2 ** 65");

my $x = Math::BigInt->new( '777' ) ** 256;
my $base = Math::BigInt->new( '12345678901234' );
is ($x->copy()->blog($base), 56, 'blog(777**256, 12345678901234)');

$x = Math::BigInt->new( '777' ) ** 777;
$base = Math::BigInt->new( '777' );
is ($x->copy()->blog($base), 777, 'blog(777**777, 777)');

#############################################################################
# test for bug in bsqrt() not taking negative _e into account
test_bpow ('200','0.5',10,      '14.14213562');
test_bpow ('20','0.5',10,       '4.472135955');
test_bpow ('2','0.5',10,        '1.414213562');
test_bpow ('0.2','0.5',10,      '0.4472135955');
test_bpow ('0.02','0.5',10,     '0.1414213562');
test_bpow ('0.49','0.5',undef , '0.7');
test_bpow ('0.49','0.5',10 ,    '0.7000000000');
test_bpow ('0.002','0.5',10,    '0.04472135955');
test_bpow ('0.0002','0.5',10,   '0.01414213562');
test_bpow ('0.0049','0.5',undef,'0.07');
test_bpow ('0.0049','0.5',10 ,  '0.07000000000');
test_bpow ('0.000002','0.5',10, '0.001414213562');
test_bpow ('0.021','0.5',10,    '0.1449137675');
test_bpow ('1.2','0.5',10,      '1.095445115');
test_bpow ('1.23','0.5',10,     '1.109053651');
test_bpow ('12.3','0.5',10,     '3.507135583');

test_bpow ('9.9','0.5',10,        '3.146426545');
test_bpow ('9.86902225','0.5',10, '3.141500000');
test_bpow ('9.86902225','0.5',undef, '3.1415');

test_bpow ('0.2','0.41',10,   '0.5169187652');

#############################################################################
# test bexp() with cached results

is ($cl->new(1)->bexp(), '2.718281828459045235360287471352662497757', 'bexp(1)');
is ($cl->new(2)->bexp(40), $cl->new(1)->bexp(45)->bpow(2,40), 'bexp(2)'); 

is ($cl->new("12.5")->bexp(61), $cl->new(1)->bexp(65)->bpow(12.5,61), 'bexp(12.5)'); 

#############################################################################
# test bexp() with big values (non-cached)

is ($cl->new(1)->bexp(100), 
  '2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427',
 'bexp(100)');

is ($cl->new("12.5")->bexp(91), $cl->new(1)->bexp(95)->bpow(12.5,91), 
  'bexp(12.5) to 91 digits'); 

# all done
1;

#############################################################################
sub test_bpow
  {
  my ($x,$y,$scale,$result) = @_;

  print "# Tried: $x->bpow($y,$scale);\n"
   unless ok ($cl->new($x)->bpow($y,$scale),$result);
  }


