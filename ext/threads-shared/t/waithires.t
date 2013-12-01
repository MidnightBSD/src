use strict;
use warnings;

BEGIN {
    # Import test.pl into its own package
    {
        package Test;
        require($ENV{PERL_CORE} ? './test.pl' : './t/test.pl');
    }

    use Config;
    if (! $Config{'useithreads'}) {
        Test::skip_all(q/Perl not compiled with 'useithreads'/);
    }

    if (! eval 'use Time::HiRes "time"; 1') {
        Test::skip_all('Time::HiRes not available');
    }
}

use ExtUtils::testlib;

sub ok {
    my ($id, $ok, $name) = @_;

    # You have to do it this way or VMS will get confused.
    if ($ok) {
        print("ok $id - $name\n");
    } else {
        print("not ok $id - $name\n");
        printf("# Failed test at line %d\n", (caller)[2]);
    }

    return ($ok);
}

BEGIN {
    $| = 1;
    print("1..57\n");   ### Number of tests that will be run ###
};

use threads;
use threads::shared;

Test::watchdog(60);   # In case we get stuck

my $TEST = 1;
ok($TEST++, 1, 'Loaded');

### Start of Testing ###

# subsecond cond_timedwait extended tests adapted from wait.t

# The two skips later on in these tests refer to this quote from the
# pod/perl583delta.pod:
#
# =head1 Platform Specific Problems
#
# The regression test ext/threads/shared/t/wait.t fails on early RedHat 9
# and HP-UX 10.20 due to bugs in their threading implementations.
# RedHat users should see https://rhn.redhat.com/errata/RHBA-2003-136.html
# and consider upgrading their glibc.


# - TEST basics

my @wait_how = (
    "simple",  # cond var == lock var; implicit lock; e.g.: cond_wait($c)
    "repeat",  # cond var == lock var; explicit lock; e.g.: cond_wait($c, $c)
    "twain"    # cond var != lock var; explicit lock; e.g.: cond_wait($c, $l)
);


SYNC_SHARED: {
    my $test_type :shared;   # simple|repeat|twain

    my $cond :shared;
    my $lock :shared;

    ok($TEST++, 1, "Shared synchronization tests preparation");

    # - TEST cond_timedwait success

    sub signaller
    {
        my $testno = $_[0];

        ok($testno++, 1, "$test_type: child before lock");
        $test_type =~ /twain/ ? lock($lock) : lock($cond);
        ok($testno++, 1, "$test_type: child obtained lock");

        if ($test_type =~ 'twain') {
            no warnings 'threads';   # lock var != cond var, so disable warnings
            cond_signal($cond);
        } else {
            cond_signal($cond);
        }
        ok($testno++, 1, "$test_type: child signalled condition");

        return($testno);
    }

    sub ctw_ok
    {
        my ($testnum, $to) = @_;

        # Which lock to obtain?
        $test_type =~ /twain/ ? lock($lock) : lock($cond);
        ok($testnum++, 1, "$test_type: obtained initial lock");

        my $thr = threads->create(\&signaller, $testnum);
        my $ok = 0;
        for ($test_type) {
            $ok = cond_timedwait($cond, time() + $to), last        if /simple/;
            $ok = cond_timedwait($cond, time() + $to, $cond), last if /repeat/;
            $ok = cond_timedwait($cond, time() + $to, $lock), last if /twain/;
            die "$test_type: unknown test\n";
        }
        $testnum = $thr->join();
        ok($testnum++, $ok, "$test_type: condition obtained");

        return ($testnum);
    }

    foreach (@wait_how) {
        $test_type = "cond_timedwait [$_]";
        my $thr = threads->create(\&ctw_ok, $TEST, 0.1);
        $TEST = $thr->join();
    }

    # - TEST cond_timedwait timeout

    sub ctw_fail
    {
        my ($testnum, $to) = @_;

        if ($^O eq "hpux" && $Config{osvers} <= 10.20) {
            # The lock obtaining would pass, but the wait will not.
            ok($testnum++, 1, "$test_type: obtained initial lock");
            ok($testnum++, 0, "# SKIP see perl583delta");

        } else {
            $test_type =~ /twain/ ? lock($lock) : lock($cond);
            ok($testnum++, 1, "$test_type: obtained initial lock");
            my $ok;
            for ($test_type) {
                $ok = cond_timedwait($cond, time() + $to), last        if /simple/;
                $ok = cond_timedwait($cond, time() + $to, $cond), last if /repeat/;
                $ok = cond_timedwait($cond, time() + $to, $lock), last if /twain/;
                die "$test_type: unknown test\n";
            }
            ok($testnum++, ! defined($ok), "$test_type: timeout");
        }

        return ($testnum);
    }

    foreach (@wait_how) {
        $test_type = "cond_timedwait pause, timeout [$_]";
        my $thr = threads->create(\&ctw_fail, $TEST, 0.3);
        $TEST = $thr->join();
    }

    foreach (@wait_how) {
        $test_type = "cond_timedwait instant timeout [$_]";
        my $thr = threads->create(\&ctw_fail, $TEST, -0.60);
        $TEST = $thr->join();
    }

} # -- SYNCH_SHARED block


# same as above, but with references to lock and cond vars

SYNCH_REFS: {
    my $test_type :shared;   # simple|repeat|twain

    my $true_cond :shared;
    my $true_lock :shared;

    my $cond = \$true_cond;
    my $lock = \$true_lock;

    ok($TEST++, 1, "Synchronization reference tests preparation");

    # - TEST cond_timedwait success

    sub signaller2
    {
        my $testno = $_[0];

        ok($testno++, 1, "$test_type: child before lock");
        $test_type =~ /twain/ ? lock($lock) : lock($cond);
        ok($testno++, 1, "$test_type: child obtained lock");

        if ($test_type =~ 'twain') {
            no warnings 'threads';   # lock var != cond var, so disable warnings
            cond_signal($cond);
        } else {
            cond_signal($cond);
        }
        ok($testno++, 1, "$test_type: child signalled condition");

        return($testno);
    }

    sub ctw_ok2
    {
        my ($testnum, $to) = @_;

        # Which lock to obtain?
        $test_type =~ /twain/ ? lock($lock) : lock($cond);
        ok($testnum++, 1, "$test_type: obtained initial lock");

        my $thr = threads->create(\&signaller2, $testnum);
        my $ok = 0;
        for ($test_type) {
            $ok = cond_timedwait($cond, time() + $to), last        if /simple/;
            $ok = cond_timedwait($cond, time() + $to, $cond), last if /repeat/;
            $ok = cond_timedwait($cond, time() + $to, $lock), last if /twain/;
            die "$test_type: unknown test\n";
        }
        $testnum = $thr->join();
        ok($testnum++, $ok, "$test_type: condition obtained");

        return ($testnum);
    }

    foreach (@wait_how) {
        $test_type = "cond_timedwait [$_]";
        my $thr = threads->create(\&ctw_ok2, $TEST, 0.05);
        $TEST = $thr->join();
    }

    # - TEST cond_timedwait timeout

    sub ctw_fail2
    {
        my ($testnum, $to) = @_;

        if ($^O eq "hpux" && $Config{osvers} <= 10.20) {
            # The lock obtaining would pass, but the wait will not.
            ok($testnum++, 1, "$test_type: obtained initial lock");
            ok($testnum++, 0, "# SKIP see perl583delta");

        } else {
            $test_type =~ /twain/ ? lock($lock) : lock($cond);
            ok($testnum++, 1, "$test_type: obtained initial lock");
            my $ok;
            for ($test_type) {
                $ok = cond_timedwait($cond, time() + $to), last        if /simple/;
                $ok = cond_timedwait($cond, time() + $to, $cond), last if /repeat/;
                $ok = cond_timedwait($cond, time() + $to, $lock), last if /twain/;
                die "$test_type: unknown test\n";
            }
            ok($testnum++, ! defined($ok), "$test_type: timeout");
        }

        return ($testnum);
    }

    foreach (@wait_how) {
        $test_type = "cond_timedwait pause, timeout [$_]";
        my $thr = threads->create(\&ctw_fail2, $TEST, 0.3);
        $TEST = $thr->join();
    }

    foreach (@wait_how) {
        $test_type = "cond_timedwait instant timeout [$_]";
        my $thr = threads->create(\&ctw_fail2, $TEST, -0.60);
        $TEST = $thr->join();
    }

} # -- SYNCH_REFS block

# Done
exit(0);

# EOF
