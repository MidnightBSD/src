#!/usr/bin/perl -w

# test calling conventions, and :constant overloading

use strict;
use Test;

BEGIN 
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/calling.t//i;
  if ($ENV{PERL_CORE})
    {
    # testing with the core distribution
    @INC = qw(../lib lib);
    }
  else
    {
    unshift @INC, '../lib';
    }
  if (-d 't')
    {
    chdir 't';
    require File::Spec;
    unshift @INC, File::Spec->catdir(File::Spec->updir, $location);
    }
  else
    {
    unshift @INC, $location;
    }
  print "# INC = @INC\n";
  my $tests = 160;
  plan tests => $tests;
  if ($] < 5.006)
    {
    for (1..$tests) { skip (1,'Not supported on older Perls'); }
    exit;
    }
  }

package Math::BigInt::Test;

use Math::BigInt;
use vars qw/@ISA/;
@ISA = qw/Math::BigInt/;		# child of MBI
use overload;

package Math::BigFloat::Test;

use Math::BigFloat;
use vars qw/@ISA/;
@ISA = qw/Math::BigFloat/;		# child of MBI
use overload;

package main;

use Math::BigInt try => 'Calc';
use Math::BigFloat;

my ($x,$y,$z,$u);
my $version = '1.76';	# adjust manually to match latest release

###############################################################################
# check whether op's accept normal strings, even when inherited by subclasses

# do one positive and one negative test to avoid false positives by "accident"

my ($func,@args,$ans,$rc,$class,$try);
while (<DATA>)
  {
  $_ =~ s/[\n\r]//g;	# remove newlines
  next if /^#/; # skip comments
  if (s/^&//)
    {
    $func = $_;
    }
  else
    {
    @args = split(/:/,$_,99);
    $ans = pop @args;
    foreach $class (qw/
      Math::BigInt Math::BigFloat Math::BigInt::Test Math::BigFloat::Test/)
      {
      $try = "'$args[0]'"; 			# quote it
      $try = $args[0] if $args[0] =~ /'/;	# already quoted
      $try = '' if $args[0] eq '';		# undef, no argument
      $try = "$class\->$func($try);";
      $rc = eval $try;
      print "# Tried: '$try'\n" if !ok ($rc, $ans);
      }
    } 

  }

$class = 'Math::BigInt';

# XXX TODO this test does not work/fail.
# test whether use Math::BigInt qw/version/ works
#$try = "use $class ($version.'1');";
#$try .= ' $x = $class->new(123); $x = "$x";';
#eval $try;
#ok_undef ( $x );               # should result in error!

# test whether fallback to calc works
$try = "use $class ($version,'try','foo, bar , ');";
$try .= "$class\->config()->{lib};";
$ans = eval $try;
ok ( $ans =~ /^Math::BigInt::(Fast)?Calc\z/, 1);

# test whether constant works or not, also test for qw($version)
# bgcd() is present in subclass, too
$try = "use Math::BigInt ($version,'bgcd',':constant');";
$try .= ' $x = 2**150; bgcd($x); $x = "$x";';
$ans = eval $try;
ok ( $ans, "1427247692705959881058285969449495136382746624");

# test wether Math::BigInt::Scalar via use works (w/ dff. spellings of calc)
$try = "use $class ($version,'lib','Scalar');";
$try .= ' $x = 2**10; $x = "$x";';
$ans = eval $try; ok ( $ans, "1024");
$try = "use $class ($version,'lib','$class\::Scalar');";
$try .= ' $x = 2**10; $x = "$x";';
$ans = eval $try; ok ( $ans, "1024");

# all done

__END__
&is_zero
1:0
0:1
&is_one
1:1
0:0
&is_positive
1:1
-1:0
&is_negative
1:0
-1:1
&is_nan
abc:1
1:0
&is_inf
inf:1
0:0
&bstr
5:5
10:10
-10:-10
abc:NaN
'+inf':inf
'-inf':-inf
&bsstr
1:1e+0
0:0e+1
2:2e+0
200:2e+2
-5:-5e+0
-100:-1e+2
abc:NaN
'+inf':inf
&babs
-1:1
1:1
&bnot
-2:1
1:-2
&bzero
:0
&bnan
:NaN
abc:NaN
&bone
:1
'+':1
'-':-1
&binf
:inf
'+':inf
'-':-inf
