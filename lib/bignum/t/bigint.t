#!/usr/bin/perl -w

###############################################################################

use Test;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 51;
  }

use bigint qw/hex oct/;

###############################################################################
# _constant tests

foreach (qw/ 
  123:123
  123.4:123
  1.4:1
  0.1:0
  -0.1:0
  -1.1:-1
  -123.4:-123
  -123:-123
  123e2:123e2
  123e-1:12
  123e-4:0
  123e-3:0
  123.345e-1:12
  123.456e+2:12345
  1234.567e+3:1234567
  1234.567e+4:1234567E1
  1234.567e+6:1234567E3
  /)
  {
  my ($x,$y) = split /:/;
  print "# Try $x\n";
  ok (bigint::_float_constant("$x"),"$y");
  }

foreach (qw/ 
  0100:64
  0200:128
  0x100:256
  0b1001:9
  /)
  {
  my ($x,$y) = split /:/;
  print "# Try $x\n";
  ok (bigint::_binary_constant("$x"),"$y");
  }

###############################################################################
# general tests

my $x = 5; ok (ref($x) =~ /^Math::BigInt/);		# :constant

# todo:  ok (2 + 2.5,4.5);				# should still work
# todo: $x = 2 + 3.5; ok (ref($x),'Math::BigFloat');

$x = 2 ** 255; ok (ref($x) =~ /^Math::BigInt/);

ok (12->bfac(),479001600);
ok (9/4,2);

ok (4.5+4.5,8);					# truncate
ok (ref(4.5+4.5) =~ /^Math::BigInt/);


###############################################################################
# accurarcy and precision

ok_undef (bigint->accuracy());
ok (bigint->accuracy(12),12);
ok (bigint->accuracy(),12);

ok_undef (bigint->precision());
ok (bigint->precision(12),12);
ok (bigint->precision(),12);

ok (bigint->round_mode(),'even');
ok (bigint->round_mode('odd'),'odd');
ok (bigint->round_mode(),'odd');

###############################################################################
# hex() and oct()

my $c = 'Math::BigInt';

ok (ref(hex(1)), $c);
ok (ref(hex(0x1)), $c);
ok (ref(hex("af")), $c);
ok (hex("af"), Math::BigInt->new(0xaf));
ok (ref(hex("0x1")), $c);

ok (ref(oct("0x1")), $c);
ok (ref(oct("01")), $c);
ok (ref(oct("0b01")), $c);
ok (ref(oct("1")), $c);
ok (ref(oct(" 1")), $c);
ok (ref(oct(" 0x1")), $c);

ok (ref(oct(0x1)), $c);
ok (ref(oct(01)), $c);
ok (ref(oct(0b01)), $c);
ok (ref(oct(1)), $c);

###############################################################################
###############################################################################
# Perl 5.005 does not like ok ($x,undef)

sub ok_undef
  {
  my $x = shift;

  ok (1,1) and return if !defined $x;
  ok ($x,'undef');
  }
