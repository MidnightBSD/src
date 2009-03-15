#!./perl

# From Tom Phoenix <rootbeer@teleport.com> 22 Feb 1997
# Based upon a test script by kgb@ast.cam.ac.uk (Karl Glazebrook)

# Looking for the hints? You're in the right place. 
# The hints are near each test, so search for "TEST #", where
# the pound sign is replaced by the number of the test.

# I'd like to include some more robust tests, but anything
# too subtle to be detected here would require a time-consuming
# test. Also, of course, we're here to detect only flaws in Perl;
# if there are flaws in the underlying system rand, that's not
# our responsibility. But if you want better tests, see
# The Art of Computer Programming, Donald E. Knuth, volume 2,
# chapter 3. ISBN 0-201-03822-6 (v. 2)

BEGIN {
    chdir "t" if -d "t";
    @INC = qw(. ../lib);
}

use strict;
use Config;

require "test.pl";
plan(tests => 8);


my $reps = 15000;	# How many times to try rand each time.
			# May be changed, but should be over 500.
			# The more the better! (But slower.)

sub bits ($) {
    # Takes a small integer and returns the number of one-bits in it.
    my $total;
    my $bits = sprintf "%o", $_[0];
    while (length $bits) {
	$total += (0,1,1,2,1,2,2,3)[chop $bits];	# Oct to bits
    }
    $total;
}

# First, let's see whether randbits is set right
{
    my($max, $min, $sum);	# Characteristics of rand
    my($off, $shouldbe);	# Problems with randbits
    my($dev, $bits);		# Number of one bits
    my $randbits = $Config{randbits};
    $max = $min = rand(1);
    for (1..$reps) {
	my $n = rand(1);
	if ($n < 0.0 or $n >= 1.0) {
	    print <<EOM;
# WHOA THERE!  \$Config{drand01} is set to '$Config{drand01}',
# but that apparently produces values < 0.0 or >= 1.0.
# Make sure \$Config{drand01} is a valid expression in the
# C-language, and produces values in the range [0.0,1.0).
#
# I give up.
EOM
	    exit;
	}
	$sum += $n;
	$bits += bits($n * 256);	# Don't be greedy; 8 is enough
		    # It's too many if randbits is less than 8!
		    # But that should never be the case... I hope.
		    # Note: If you change this, you must adapt the
		    # formula for absolute standard deviation, below.
	$max = $n if $n > $max;
	$min = $n if $n < $min;
    }


    # This test checks for one of Perl's most frequent
    # mis-configurations. Your system's documentation
    # for rand(2) should tell you what value you need
    # for randbits. Usually the diagnostic message
    # has the right value as well. Just fix it and
    # recompile, and you'll usually be fine. (The main 
    # reason that the diagnostic message might get the
    # wrong value is that Config.pm is incorrect.)
    #
    unless (ok( !$max <= 0 or $max >= (2 ** $randbits))) {# Just in case...
	print <<DIAG;
# max=[$max] min=[$min]
# This perl was compiled with randbits=$randbits
# which is _way_ off. Or maybe your system rand is broken,
# or your C compiler can't multiply, or maybe Martians
# have taken over your computer. For starters, see about
# trying a better value for randbits, probably smaller.
DIAG

	# If that isn't the problem, we'll have
	# to put d_martians into Config.pm 
	print "# Skipping remaining tests until randbits is fixed.\n";
	exit;
    }

    $off = log($max) / log(2);			# log2
    $off = int($off) + ($off > 0);		# Next more positive int
    unless (is( $off, 0 )) {
	$shouldbe = $Config{randbits} + $off;
	print "# max=[$max] min=[$min]\n";
	print "# This perl was compiled with randbits=$randbits on $^O.\n";
	print "# Consider using randbits=$shouldbe instead.\n";
	# And skip the remaining tests; they would be pointless now.
	print "# Skipping remaining tests until randbits is fixed.\n";
	exit;
    }


    # This should always be true: 0 <= rand(1) < 1
    # If this test is failing, something is seriously wrong,
    # either in perl or your system's rand function.
    #
    unless (ok( !($min < 0 or $max >= 1) )) {	# Slightly redundant...
	print "# min too low\n" if $min < 0;
	print "# max too high\n" if $max >= 1;
    }


    # This is just a crude test. The average number produced
    # by rand should be about one-half. But once in a while
    # it will be relatively far away. Note: This test will
    # occasionally fail on a perfectly good system!
    # See the hints for test 4 to see why.
    #
    $sum /= $reps;
    unless (ok( !($sum < 0.4 or $sum > 0.6) )) {
	print "# Average random number is far from 0.5\n";
    }


    #   NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
    # This test will fail .1% of the time on a normal system.
    #				also
    # This test asks you to see these hints 100% of the time!
    #   NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
    #
    # There is probably no reason to be alarmed that
    # something is wrong with your rand function. But,
    # if you're curious or if you can't help being 
    # alarmed, keep reading.
    #
    # This is a less-crude test than test 3. But it has
    # the same basic flaw: Unusually distributed random
    # values should occasionally appear in every good
    # random number sequence. (If you flip a fair coin
    # twenty times every day, you'll see it land all
    # heads about one time in a million days, on the
    # average. That might alarm you if you saw it happen
    # on the first day!)
    #
    # So, if this test failed on you once, run it a dozen
    # times. If it keeps failing, it's likely that your
    # rand is bogus. If it keeps passing, it's likely
    # that the one failure was bogus. If it's a mix,
    # read on to see about how to interpret the tests.
    #
    # The number printed in square brackets is the
    # standard deviation, a statistical measure
    # of how unusual rand's behavior seemed. It should
    # fall in these ranges with these *approximate*
    # probabilities:
    #
    #		under 1		68.26% of the time
    #		1-2		27.18% of the time
    #		2-3		 4.30% of the time
    #		over 3		 0.26% of the time
    #
    # If the numbers you see are not scattered approximately
    # (not exactly!) like that table, check with your vendor
    # to find out what's wrong with your rand. Or with this
    # algorithm. :-)
    #
    # Calculating absoulute standard deviation for number of bits set
    # (eight bits per rep)
    $dev = abs ($bits - $reps * 4) / sqrt($reps * 2);

    ok( $dev < 3.3 );

    if ($dev < 1.96) {
	print "# Your rand seems fine. If this test failed\n";
	print "# previously, you may want to run it again.\n";
    } elsif ($dev < 2.575) {
	print "# This is ok, but suspicious. But it will happen\n";
	print "# one time out of 25, more or less.\n";
	print "# You should run this test again to be sure.\n";
    } elsif ($dev < 3.3) {
	print "# This is very suspicious. It will happen only\n";
	print "# about one time out of 100, more or less.\n";
	print "# You should run this test again to be sure.\n";
    } elsif ($dev < 3.9) {
	print "# This is VERY suspicious. It will happen only\n";
	print "# about one time out of 1000, more or less.\n";
	print "# You should run this test again to be sure.\n";
    } else {
	print "# This is VERY VERY suspicious.\n";
	print "# Your rand seems to be bogus.\n";
    }
    print "#\n# If you are having random number troubles,\n";
    print "# see the hints within the test script for more\n";
    printf "# information on why this might fail. [ %.3f ]\n", $dev;
}


# Now, let's see whether rand accepts its argument
{
    my($max, $min);
    $max = $min = rand(100);
    for (1..$reps) {
	my $n = rand(100);
	$max = $n if $n > $max;
	$min = $n if $n < $min;
    }

    # This test checks to see that rand(100) really falls 
    # within the range 0 - 100, and that the numbers produced
    # have a reasonably-large range among them.
    #
    unless ( ok( !($min < 0 or $max >= 100 or ($max - $min) < 65) ) ) {
	print "# min too low\n" if $min < 0;
	print "# max too high\n" if $max >= 100;
	print "# range too narrow\n" if ($max - $min) < 65;
    }


    # This test checks that rand without an argument
    # is equivalent to rand(1).
    #
    $_ = 12345;		# Just for fun.
    srand 12345;
    my $r = rand;
    srand 12345;
    is(rand(1),  $r,  'rand() without args is rand(1)');


    # This checks that rand without an argument is not
    # rand($_). (In case somebody got overzealous.)
    # 
    ok($r < 1,        'rand() without args is under 1');
}

