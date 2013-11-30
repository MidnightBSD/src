
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
BEGIN { plan tests => 40 };

use strict;
use warnings;
use Unicode::Collate::Locale;

ok(1);

#########################

my $objTn = Unicode::Collate::Locale->
    new(locale => 'TN', normalization => undef);

ok($objTn->getlocale, 'tn');

$objTn->change(level => 1);

ok($objTn->lt("e", "e\x{302}"));
ok($objTn->gt("f", "e\x{302}"));
ok($objTn->lt("o", "o\x{302}"));
ok($objTn->gt("p", "o\x{302}"));
ok($objTn->lt("s", "s\x{30C}"));
ok($objTn->gt("t", "s\x{30C}"));

# 8

$objTn->change(level => 2);

ok($objTn->eq("e\x{302}", "E\x{302}"));
ok($objTn->eq("o\x{302}", "O\x{302}"));
ok($objTn->eq("s\x{30C}", "S\x{30C}"));

$objTn->change(level => 3);

ok($objTn->lt("e\x{302}", "E\x{302}"));
ok($objTn->lt("o\x{302}", "O\x{302}"));
ok($objTn->lt("s\x{30C}", "S\x{30C}"));

# 14

ok($objTn->eq("e\x{302}", pack('U', 0xEA)));
ok($objTn->eq("E\x{302}", pack('U', 0xCA)));
ok($objTn->eq("o\x{302}", pack('U', 0xF4)));
ok($objTn->eq("O\x{302}", pack('U', 0xD4)));
ok($objTn->eq("s\x{30C}", "\x{161}"));
ok($objTn->eq("S\x{30C}", "\x{160}"));

# 20

ok($objTn->eq("e\x{302}\x{300}", "\x{1EC1}"));
ok($objTn->eq("E\x{302}\x{300}", "\x{1EC0}"));
ok($objTn->eq("e\x{302}\x{301}", "\x{1EBF}"));
ok($objTn->eq("E\x{302}\x{301}", "\x{1EBE}"));
ok($objTn->eq("e\x{302}\x{303}", "\x{1EC5}"));
ok($objTn->eq("E\x{302}\x{303}", "\x{1EC4}"));
ok($objTn->eq("e\x{302}\x{309}", "\x{1EC3}"));
ok($objTn->eq("E\x{302}\x{309}", "\x{1EC2}"));
ok($objTn->eq("e\x{302}\x{323}", "\x{1EC7}"));
ok($objTn->eq("E\x{302}\x{323}", "\x{1EC6}"));

ok($objTn->eq("o\x{302}\x{300}", "\x{1ED3}"));
ok($objTn->eq("O\x{302}\x{300}", "\x{1ED2}"));
ok($objTn->eq("o\x{302}\x{301}", "\x{1ED1}"));
ok($objTn->eq("O\x{302}\x{301}", "\x{1ED0}"));
ok($objTn->eq("o\x{302}\x{303}", "\x{1ED7}"));
ok($objTn->eq("O\x{302}\x{303}", "\x{1ED6}"));
ok($objTn->eq("o\x{302}\x{309}", "\x{1ED5}"));
ok($objTn->eq("O\x{302}\x{309}", "\x{1ED4}"));
ok($objTn->eq("o\x{302}\x{323}", "\x{1ED9}"));
ok($objTn->eq("O\x{302}\x{323}", "\x{1ED8}"));

# 40
