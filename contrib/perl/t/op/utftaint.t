#!./perl -T
# tests whether tainting works with UTF-8

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib);
}

use strict;
use Config;

# How to identify taint when you see it
sub any_tainted (@) {
    not eval { join("",@_), kill 0; 1 };
}
sub tainted ($) {
    any_tainted @_;
}

require './test.pl';
plan(tests => 3*10 + 3*8 + 2*16 + 2);

my $arg = $ENV{PATH}; # a tainted value
use constant UTF8 => "\x{1234}";

*is_utf8 = \&utf8::is_utf8;

for my $ary ([ascii => 'perl'], [latin1 => "\xB6"], [utf8 => "\x{100}"]) {
    my $encode = $ary->[0];
    my $string = $ary->[1];

    my $taint = $arg; substr($taint, 0) = $ary->[1];

    is(tainted($taint), tainted($arg), "tainted: $encode, before test");

    my $lconcat = $taint;
       $lconcat .= UTF8;
    is($lconcat, $string.UTF8, "compare: $encode, concat left");

    is(tainted($lconcat), tainted($arg), "tainted: $encode, concat left");

    my $rconcat = UTF8;
       $rconcat .= $taint;
    is($rconcat, UTF8.$string, "compare: $encode, concat right");

    is(tainted($rconcat), tainted($arg), "tainted: $encode, concat right");

    my $ljoin = join('!', $taint, UTF8);
    is($ljoin, join('!', $string, UTF8), "compare: $encode, join left");

    is(tainted($ljoin), tainted($arg), "tainted: $encode, join left");

    my $rjoin = join('!', UTF8, $taint);
    is($rjoin, join('!', UTF8, $string), "compare: $encode, join right");

    is(tainted($rjoin), tainted($arg), "tainted: $encode, join right");

    is(tainted($taint), tainted($arg), "tainted: $encode, after test");
}


for my $ary ([ascii => 'perl'], [latin1 => "\xB6"], [utf8 => "\x{100}"]) {
    my $encode = $ary->[0];

    my $utf8 = pack('U*') . $ary->[1];
    my $byte = unpack('U0a*', $utf8);

    my $taint = $arg; substr($taint, 0) = $utf8;
    utf8::encode($taint);

    is($taint, $byte, "compare: $encode, encode utf8");

    is(pack('a*',$taint), pack('a*',$byte), "bytecmp: $encode, encode utf8");

    ok(!is_utf8($taint), "is_utf8: $encode, encode utf8");

    is(tainted($taint), tainted($arg), "tainted: $encode, encode utf8");

    my $taint = $arg; substr($taint, 0) = $byte;
    utf8::decode($taint);

    is($taint, $utf8, "compare: $encode, decode byte");

    is(pack('a*',$taint), pack('a*',$utf8), "bytecmp: $encode, decode byte");

    is(is_utf8($taint), ($encode ne 'ascii'), "is_utf8: $encode, decode byte");

    is(tainted($taint), tainted($arg), "tainted: $encode, decode byte");
}


for my $ary ([ascii => 'perl'], [latin1 => "\xB6"]) {
    my $encode = $ary->[0];

    my $up   = pack('U*') . $ary->[1];
    my $down = pack("a*", $ary->[1]);

    my $taint = $arg; substr($taint, 0) = $up;
    utf8::upgrade($taint);

    is($taint, $up, "compare: $encode, upgrade up");

    is(pack('a*',$taint), pack('a*',$up), "bytecmp: $encode, upgrade up");

    ok(is_utf8($taint), "is_utf8: $encode, upgrade up");

    is(tainted($taint), tainted($arg), "tainted: $encode, upgrade up");

    my $taint = $arg; substr($taint, 0) = $down;
    utf8::upgrade($taint);

    is($taint, $up, "compare: $encode, upgrade down");

    is(pack('a*',$taint), pack('a*',$up), "bytecmp: $encode, upgrade down");

    ok(is_utf8($taint), "is_utf8: $encode, upgrade down");

    is(tainted($taint), tainted($arg), "tainted: $encode, upgrade down");

    my $taint = $arg; substr($taint, 0) = $up;
    utf8::downgrade($taint);

    is($taint, $down, "compare: $encode, downgrade up");

    is(pack('a*',$taint), pack('a*',$down), "bytecmp: $encode, downgrade up");

    ok(!is_utf8($taint), "is_utf8: $encode, downgrade up");

    is(tainted($taint), tainted($arg), "tainted: $encode, downgrade up");

    my $taint = $arg; substr($taint, 0) = $down;
    utf8::downgrade($taint);

    is($taint, $down, "compare: $encode, downgrade down");

    is(pack('a*',$taint), pack('a*',$down), "bytecmp: $encode, downgrade down");

    ok(!is_utf8($taint), "is_utf8: $encode, downgrade down");

    is(tainted($taint), tainted($arg), "tainted: $encode, downgrade down");
}

{
    fresh_perl_is('$a = substr $^X, 0, 0; /\x{100}/i; /$a\x{100}/i || print q,ok,',
		  'ok', {switches => ["-T", "-l"]},
		  "matching a regexp is taint agnostic");

    fresh_perl_is('$a = substr $^X, 0, 0; /$a\x{100}/i || print q,ok,',
		  'ok', {switches => ["-T", "-l"]},
		  "therefore swash_init should be taint agnostic");
}
