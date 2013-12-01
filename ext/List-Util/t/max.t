#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use strict;
use Test::More tests => 8;
use List::Util qw(max);

my $v;

ok(defined &max, 'defined');

$v = max(1);
is($v, 1, 'single arg');

$v = max (1,2);
is($v, 2, '2-arg ordered');

$v = max(2,1);
is($v, 2, '2-arg reverse ordered');

my @a = map { rand() } 1 .. 20;
my @b = sort { $a <=> $b } @a;
$v = max(@a);
is($v, $b[-1], '20-arg random order');

my $one = Foo->new(1);
my $two = Foo->new(2);
my $thr = Foo->new(3);

$v = max($one,$two,$thr);
is($v, 3, 'overload');

$v = max($thr,$two,$one);
is($v, 3, 'overload');

{ package Foo;

use overload
  '""' => sub { ${$_[0]} },
  '+0' => sub { ${$_[0]} },
  fallback => 1;
  sub new {
    my $class = shift;
    my $value = shift;
    bless \$value, $class;
  }
}

SKIP: {
  eval { require bignum; } or skip("Need bignum for testing overloading",1);

  my $v1 = 2**65;
  my $v2 = $v1 - 1;
  my $v3 = $v2 - 1;
  $v = max($v1,$v2,$v1,$v3,$v1);
  is($v, $v1, 'bigint');
}
