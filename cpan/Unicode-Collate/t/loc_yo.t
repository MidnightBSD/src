
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
BEGIN { plan tests => 33 };

use strict;
use warnings;
use Unicode::Collate::Locale;

ok(1);

#########################

my $objYo = Unicode::Collate::Locale->
    new(locale => 'YO', normalization => undef);

ok($objYo->getlocale, 'yo');

$objYo->change(level => 1);

ok($objYo->lt("e", "e\x{323}"));
ok($objYo->gt("f", "e\x{323}"));
ok($objYo->lt("g", "gb"));
ok($objYo->lt("gz","gb"));
ok($objYo->gt("h", "gb"));
ok($objYo->lt("o", "o\x{323}"));
ok($objYo->gt("p", "o\x{323}"));
ok($objYo->lt("s", "s\x{323}"));
ok($objYo->gt("t", "s\x{323}"));

# 11

$objYo->change(level => 2);

ok($objYo->eq("e\x{323}", "E\x{323}"));
ok($objYo->eq("gb", "Gb"));
ok($objYo->eq("Gb", "GB"));
ok($objYo->eq("o\x{323}", "O\x{323}"));
ok($objYo->eq("s\x{323}", "S\x{323}"));

# 16

$objYo->change(level => 3);

ok($objYo->lt("e\x{323}", "E\x{323}"));
ok($objYo->lt("gb", "Gb"));
ok($objYo->lt("Gb", "GB"));
ok($objYo->lt("o\x{323}", "O\x{323}"));
ok($objYo->lt("s\x{323}", "S\x{323}"));

# 21

ok($objYo->eq("e\x{323}", "\x{1EB9}"));
ok($objYo->eq("E\x{323}", "\x{1EB8}"));
ok($objYo->eq("o\x{323}", "\x{1ECD}"));
ok($objYo->eq("O\x{323}", "\x{1ECC}"));
ok($objYo->eq("s\x{323}", "\x{1E63}"));
ok($objYo->eq("S\x{323}", "\x{1E62}"));

ok($objYo->eq("e\x{323}\x{302}", "\x{1EC7}"));
ok($objYo->eq("E\x{323}\x{302}", "\x{1EC6}"));
ok($objYo->eq("o\x{323}\x{302}", "\x{1ED9}"));
ok($objYo->eq("O\x{323}\x{302}", "\x{1ED8}"));
ok($objYo->eq("o\x{323}\x{31B}", "\x{1EE3}"));
ok($objYo->eq("O\x{323}\x{31B}", "\x{1EE2}"));

# 33
