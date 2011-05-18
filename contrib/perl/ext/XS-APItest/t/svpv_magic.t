#!perl -w

use Test::More tests => 5;

BEGIN {
    use_ok('XS::APItest')
};

$b = "\303\244"; # or encode_utf8("\x{e4}");

is(XS::APItest::first_byte($b), 0303,
    "test function first_byte works");

$b =~ /(.)/;
is(XS::APItest::first_byte($1), 0303,
    "matching works correctly");

$a = qq[\x{263a}]; # utf8 flag is set

$a =~ s/(.)/$1/;      # $1 now has the utf8 flag set too
$b =~ /(.)/;          # $1 shouldn't have the utf8 flag anymore

is(XS::APItest::first_byte("$1"), 0303,
    "utf8 flag in match fetched correctly when stringified first");

$a =~ s/(.)/$1/;      # $1 now has the utf8 flag set too
$b =~ /(.)/;          # $1 shouldn't have the utf8 flag anymore

is(eval { XS::APItest::first_byte($1) } || $@, 0303,
    "utf8 flag fetched correctly without stringification");
