#!/usr/bin/perl -w

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ( '../lib', '../ext/Test-Harness/t/lib' );
    }
    else {
        use lib 't/lib';
    }
}

use strict;

use Test::More tests => 1;

use Dev::Null;

use Test::Harness;

sub _all_ok {
    my ($tot) = shift;
    return $tot->{bad} == 0
      && ( $tot->{max} || $tot->{skipped} ) ? 1 : 0;
}

{
    local $ENV{PERL_TEST_HARNESS_DUMP_TAP} = 0;
    local $Test::Harness::Verbose = -9;

    push @INC, 'examples';

    tie *NULL, 'Dev::Null' or die $!;
    select NULL;
    my ( $tot, $failed ) = Test::Harness::execute_tests(
        tests => [
            $ENV{PERL_CORE}
            ? '../ext/Test-Harness/t/sample-tests/inc_taint'
            : 't/sample-tests/inc_taint'
        ]
    );
    select STDOUT;

    ok( _all_ok($tot), 'tests with taint on preserve @INC' );
}
