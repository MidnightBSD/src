#!./perl -w

require './test.pl';
use strict;

# Verify that addition/subtraction properly upgrade to doubles.
# These tests are only significant on machines with 32 bit longs,
# and two's complement negation, but shouldn't fail anywhere.

my $a = 2147483647;
my $c=$a++;
cmp_ok($a, '==', 2147483648);

$a = 2147483647;
$c=++$a;
cmp_ok($a, '==', 2147483648);

$a = 2147483647;
$a=$a+1;
cmp_ok($a, '==', 2147483648);

$a = -2147483648;
$c=$a--;
cmp_ok($a, '==', -2147483649);

$a = -2147483648;
$c=--$a;
cmp_ok($a, '==', -2147483649);

$a = -2147483648;
$a=$a-1;
cmp_ok($a, '==', -2147483649);

$a = 2147483648;
$a = -$a;
$c=$a--;
cmp_ok($a, '==', -2147483649);

$a = 2147483648;
$a = -$a;
$c=--$a;
cmp_ok($a, '==', -2147483649);

$a = 2147483648;
$a = -$a;
$a=$a-1;
cmp_ok($a, '==', -2147483649);

$a = 2147483648;
$b = -$a;
$c=$b--;
cmp_ok($b, '==', -$a-1);

$a = 2147483648;
$b = -$a;
$c=--$b;
cmp_ok($b, '==', -$a-1);

$a = 2147483648;
$b = -$a;
$b=$b-1;
cmp_ok($b, '==', -(++$a));

$a = undef;
is($a++, '0', "postinc undef returns '0'");

$a = undef;
is($a--, undef, "postdec undef returns undef");

# Verify that shared hash keys become unshared.

sub check_same {
  my ($orig, $suspect) = @_;
  my $fail;
  while (my ($key, $value) = each %$suspect) {
    if (exists $orig->{$key}) {
      if ($orig->{$key} ne $value) {
        print "# key '$key' was '$orig->{$key}' now '$value'\n";
        $fail = 1;
      }
    } else {
      print "# key '$key' is '$orig->{$key}', unexpect.\n";
      $fail = 1;
    }
  }
  foreach (keys %$orig) {
    next if (exists $suspect->{$_});
    print "# key '$_' was '$orig->{$_}' now missing\n";
    $fail = 1;
  }
  ok (!$fail);
}

my (%orig) = my (%inc) = my (%dec) = my (%postinc) = my (%postdec)
  = (1 => 1, ab => "ab");
my %up = (1=>2, ab => 'ac');
my %down = (1=>0, ab => -1);

foreach (keys %inc) {
  my $ans = $up{$_};
  my $up;
  eval {$up = ++$_};
  is($up, $ans);
  is($@, '');
}

check_same (\%orig, \%inc);

foreach (keys %dec) {
  my $ans = $down{$_};
  my $down;
  eval {$down = --$_};
  is($down, $ans);
  is($@, '');
}

check_same (\%orig, \%dec);

foreach (keys %postinc) {
  my $ans = $postinc{$_};
  my $up;
  eval {$up = $_++};
  is($up, $ans);
  is($@, '');
}

check_same (\%orig, \%postinc);

foreach (keys %postdec) {
  my $ans = $postdec{$_};
  my $down;
  eval {$down = $_--};
  is($down, $ans);
  is($@, '');
}

check_same (\%orig, \%postdec);

{
    no warnings 'uninitialized';
    my ($x, $y);
    eval {
	$y ="$x\n";
	++$x;
    };
    cmp_ok($x, '==', 1);
    is($@, '');

    my ($p, $q);
    eval {
	$q ="$p\n";
	--$p;
    };
    cmp_ok($p, '==', -1);
    is($@, '');
}

$a = 2147483648;
$c=--$a;
cmp_ok($a, '==', 2147483647);


$a = 2147483648;
$c=$a--;
cmp_ok($a, '==', 2147483647);

{
    use integer;
    my $x = 0;
    $x++;
    cmp_ok($x, '==', 1, "(void) i_postinc");
    $x--;
    cmp_ok($x, '==', 0, "(void) i_postdec");
}

# I'm sure that there's an IBM format with a 48 bit mantissa
# IEEE doubles have a 53 bit mantissa
# 80 bit long doubles have a 64 bit mantissa
# sparcs have a 112 bit mantissa for their long doubles. Just to be awkward :-)

my $h_uv_max = 1 + (~0 >> 1);
my $found;
for my $n (47..113) {
    my $power_of_2 = 2**$n;
    my $plus_1 = $power_of_2 + 1;
    next if $plus_1 != $power_of_2;
    my ($start_p, $start_n);
    if ($h_uv_max > $power_of_2 / 2) {
	my $uv_max = 1 + 2 * (~0 >> 1);
	# UV_MAX is 2**$something - 1, so subtract 1 to get the start value
	$start_p = $uv_max - 1;
	# whereas IV_MIN is -(2**$something), so subtract 2
	$start_n = -$h_uv_max + 2;
	print "# Mantissa overflows at 2**$n ($power_of_2)\n";
	print "# But max UV ($uv_max) is greater so testing that\n";
    } else {
	print "# Testing 2**$n ($power_of_2) which overflows the mantissa\n";
	$start_p = int($power_of_2 - 2);
	$start_n = -$start_p;
	my $check = $power_of_2 - 2;
	die "Something wrong with our rounding assumptions: $check vs $start_p"
	    unless $start_p == $check;
    }

    foreach ([$start_p, '++$i', 'pre-inc', 'inc'],
	     [$start_p, '$i++', 'post-inc', 'inc'],
	     [$start_n, '--$i', 'pre-dec', 'dec'],
	     [$start_n, '$i--', 'post-dec', 'dec']) {
	my ($start, $action, $description, $act) = @$_;
	my $code = eval << "EOC" or die $@;
sub {
    no warnings 'imprecision';
    my \$i = \$start;
    for(0 .. 3) {
        my \$a = $action;
    }
}
EOC

	warning_is($code, undef, "$description under no warnings 'imprecision'");

	$code = eval << "EOC" or die $@;
sub {
    use warnings 'imprecision';
    my \$i = \$start;
    for(0 .. 3) {
        my \$a = $action;
    }
}
EOC

	warnings_like($code, [(qr/Lost precision when ${act}rementing -?\d+/) x 2],
		      "$description under use warnings 'imprecision'");
    }

    $found = 1;
    last;
}
die "Could not find a value which overflows the mantissa" unless $found;

# these will segfault if they fail

sub PVBM () { 'foo' }
{ my $dummy = index 'foo', PVBM }

isnt(scalar eval { my $pvbm = PVBM; $pvbm++ }, undef);
isnt(scalar eval { my $pvbm = PVBM; $pvbm-- }, undef);
isnt(scalar eval { my $pvbm = PVBM; ++$pvbm }, undef);
isnt(scalar eval { my $pvbm = PVBM; --$pvbm }, undef);

# #9466

# don't use pad TARG when the thing you're copying is a ref, or the referent
# won't get freed.
{
    package P9466;
    my $x;
    sub DESTROY { $x = 1 }
    for (0..1) {
	$x = 0;
	my $a = bless {};
	my $b = $_ ? $a++ : $a--;
	undef $a; undef $b;
	::is($x, 1, "9466 case $_");
    }
}

done_testing();
