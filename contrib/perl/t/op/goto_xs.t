#!./perl
# tests for "goto &sub"-ing into XSUBs

# Note: This only tests things that should *work*.  At some point, it may
#       be worth while to write some failure tests for things that should
#       *break* (such as calls with wrong number of args).  For now, I'm
#       guessing that if all of these work correctly, the bad ones will
#       break correctly as well.

BEGIN { $| = 1; }
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PERL5LIB} = "../lib";

# turn warnings into fatal errors
    $SIG{__WARN__} = sub { die "WARNING: @_" } ;

    foreach (qw(Fcntl XS::APItest)) {
	eval "require $_"
	or do { print "1..0\n# $_ unavailable, can't test XS goto.\n"; exit 0 }
    }
}
print "1..11\n";

# We don't know what symbols are defined in platform X's system headers.
# We don't even want to guess, because some platform out there will
# likely do the unthinkable.  However, Fcntl::constant("LOCK_SH",0)
# should always return a value, even on platforms which don't define the
# cpp symbol; Fcntl.xs says:
#           /* We support flock() on systems which don't have it, so
#              always supply the constants. */
# If this ceases to be the case, we're in trouble. =)
$VALID = 'LOCK_SH';

### First, we check whether Fcntl::constant returns sane answers.
# Fcntl::constant("LOCK_SH",0) should always succeed.

$value = Fcntl::constant($VALID);
print((!defined $value)
      ? "not ok 1\n# Sanity check broke, remaining tests will fail.\n"
      : "ok 1\n");

### OK, we're ready to do real tests.

# test "goto &function_constant"
sub goto_const { goto &Fcntl::constant; }

$ret = goto_const($VALID);
print(($ret == $value) ? "ok 2\n" : "not ok 2\n# ($ret != $value)\n");

# test "goto &$function_package_and_name"
$FNAME1 = 'Fcntl::constant';
sub goto_name1 { goto &$FNAME1; }

$ret = goto_name1($VALID);
print(($ret == $value) ? "ok 3\n" : "not ok 3\n# ($ret != $value)\n");

# test "goto &$function_package_and_name" again, with dirtier stack
$ret = goto_name1($VALID);
print(($ret == $value) ? "ok 4\n" : "not ok 4\n# ($ret != $value)\n");
$ret = goto_name1($VALID);
print(($ret == $value) ? "ok 5\n" : "not ok 5\n# ($ret != $value)\n");

# test "goto &$function_name" from local package
package Fcntl;
$FNAME2 = 'constant';
sub goto_name2 { goto &$FNAME2; }
package main;

$ret = Fcntl::goto_name2($VALID);
print(($ret == $value) ? "ok 6\n" : "not ok 6\n# ($ret != $value)\n");

# test "goto &$function_ref"
$FREF = \&Fcntl::constant;
sub goto_ref { goto &$FREF; }

$ret = goto_ref($VALID);
print(($ret == $value) ? "ok 7\n" : "not ok 7\n# ($ret != $value)\n");

### tests where the args are not on stack but in GvAV(defgv) (ie, @_)

# test "goto &function_constant" from a sub called without arglist
sub call_goto_const { &goto_const; }

$ret = call_goto_const($VALID);
print(($ret == $value) ? "ok 8\n" : "not ok 8\n# ($ret != $value)\n");

# test "goto &$function_package_and_name" from a sub called without arglist
sub call_goto_name1 { &goto_name1; }

$ret = call_goto_name1($VALID);
print(($ret == $value) ? "ok 9\n" : "not ok 9\n# ($ret != $value)\n");

# test "goto &$function_ref" from a sub called without arglist
sub call_goto_ref { &goto_ref; }

$ret = call_goto_ref($VALID);
print(($ret == $value) ? "ok 10\n" : "not ok 10\n# ($ret != $value)\n");


# [perl #35878] croak in XS after goto segfaulted

use XS::APItest qw(mycroak);

sub goto_croak { goto &mycroak }

{
    my $e;
    for (1..4) {
	eval { goto_croak("boo$_\n") };
	$e .= $@;
    }
    print $e eq "boo1\nboo2\nboo3\nboo4\n" ? "ok 11\n" : "not ok 11\n";
}

