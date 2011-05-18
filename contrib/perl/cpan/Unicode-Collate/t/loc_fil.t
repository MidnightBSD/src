
BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
    if ($ENV{PERL_CORE}) {
	chdir('t') if -d 't';
	@INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use Test;
BEGIN { plan tests => 14 };

use strict;
use warnings;
use Unicode::Collate::Locale;

ok(1);

#########################

my $objFil = Unicode::Collate::Locale->
    new(locale => 'FIL', normalization => undef);

ok($objFil->getlocale, 'fil');

$objFil->change(level => 1);

ok($objFil->lt("n", "n\x{303}"));
ok($objFil->lt("nz","n\x{303}"));
ok($objFil->lt("n\x{303}", "ng"));
ok($objFil->gt("o", "ng"));

# 6

$objFil->change(level => 2);

ok($objFil->eq("ng", "Ng"));
ok($objFil->eq("Ng", "NG"));
ok($objFil->eq("n\x{303}", "N\x{303}"));

# 9

$objFil->change(level => 3);

ok($objFil->lt("ng", "Ng"));
ok($objFil->lt("Ng", "NG"));
ok($objFil->lt("n\x{303}", "N\x{303}"));
ok($objFil->eq("n\x{303}", pack('U', 0xF1)));
ok($objFil->eq("N\x{303}", pack('U', 0xD1)));

# 14
