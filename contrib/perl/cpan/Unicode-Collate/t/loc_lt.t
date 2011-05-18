
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
BEGIN { plan tests => 55 };

use strict;
use warnings;
use Unicode::Collate::Locale;

ok(1);

#########################

my $objLt = Unicode::Collate::Locale->
    new(locale => 'LT', normalization => undef);

ok($objLt->getlocale, 'lt');

$objLt->change(level => 1);

ok($objLt->lt("c", "c\x{30C}"));
ok($objLt->gt("d", "c\x{30C}"));
ok($objLt->lt("s", "s\x{30C}"));
ok($objLt->gt("t", "s\x{30C}"));
ok($objLt->lt("z", "z\x{30C}"));
ok($objLt->lt("z\x{30C}", "\x{292}")); # U+0292 EZH

# 8

ok($objLt->eq( "\x{328}",  "\x{307}"));
ok($objLt->eq("e\x{328}", "e\x{307}"));
ok($objLt->eq("i\x{328}", "i\x{307}"));
ok($objLt->eq('i', 'y'));

$objLt->change(level => 2);

ok($objLt->lt( "\x{328}",  "\x{307}"));
ok($objLt->lt("e\x{328}", "e\x{307}"));
ok($objLt->lt("i\x{328}", "i\x{307}"));
ok($objLt->lt('i', 'y'));

# 16

ok($objLt->eq("c\x{30C}", "C\x{30C}"));
ok($objLt->eq("s\x{30C}", "S\x{30C}"));
ok($objLt->eq("z\x{30C}", "Z\x{30C}"));
ok($objLt->eq('y', 'Y'));
ok($objLt->eq("e\x{307}", "E\x{307}"));
ok($objLt->eq("i\x{307}", "I\x{307}"));
ok($objLt->eq("a\x{328}", "A\x{328}"));
ok($objLt->eq("e\x{328}", "E\x{328}"));
ok($objLt->eq("i\x{328}", "I\x{328}"));
ok($objLt->eq("u\x{328}", "U\x{328}"));

# 26

$objLt->change(level => 3);

ok($objLt->lt("c\x{30C}", "C\x{30C}"));
ok($objLt->lt("s\x{30C}", "S\x{30C}"));
ok($objLt->lt("z\x{30C}", "Z\x{30C}"));
ok($objLt->lt('y', 'Y'));
ok($objLt->lt("e\x{307}", "E\x{307}"));
ok($objLt->lt("i\x{307}", "I\x{307}"));
ok($objLt->lt("a\x{328}", "A\x{328}"));
ok($objLt->lt("e\x{328}", "E\x{328}"));
ok($objLt->lt("i\x{328}", "I\x{328}"));
ok($objLt->lt("u\x{328}", "U\x{328}"));

# 36

ok($objLt->eq("c\x{30C}", "\x{10D}"));
ok($objLt->eq("C\x{30C}", "\x{10C}"));
ok($objLt->eq("s\x{30C}", "\x{161}"));
ok($objLt->eq("S\x{30C}", "\x{160}"));
ok($objLt->eq("z\x{30C}", "\x{17E}"));
ok($objLt->eq("Z\x{30C}", "\x{17D}"));
ok($objLt->eq("e\x{307}", "\x{117}"));
ok($objLt->eq("E\x{307}", "\x{116}"));
ok($objLt->eq("I\x{307}", "\x{130}"));

# 45

ok($objLt->eq("a\x{328}", "\x{105}"));
ok($objLt->eq("A\x{328}", "\x{104}"));
ok($objLt->eq("e\x{328}", "\x{119}"));
ok($objLt->eq("E\x{328}", "\x{118}"));
ok($objLt->eq("i\x{328}", "\x{12F}"));
ok($objLt->eq("I\x{328}", "\x{12E}"));
ok($objLt->eq("u\x{328}", "\x{173}"));
ok($objLt->eq("U\x{328}", "\x{172}"));
ok($objLt->eq("u\x{304}", "\x{16B}"));
ok($objLt->eq("U\x{304}", "\x{16A}"));

# 55
