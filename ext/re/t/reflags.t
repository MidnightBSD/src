#!./perl

BEGIN {
	require Config;
	if (($Config::Config{'extensions'} !~ /\bre\b/) ){
        	print "1..0 # Skip -- Perl configured without re module\n";
		exit 0;
	}
}

use strict;

use Test::More tests => 53;

my @flags = qw( a d l u );

use re '/i';
ok "Foo" =~ /foo/, 'use re "/i"';
no re '/i';
ok "Foo" !~ /foo/, 'no re "/i"';
use re '/x';
ok "foo" =~ / foo /, 'use re "/x"';
no re '/x';
ok "foo" !~ / foo /, 'no re "/x"';
use re '/s';
ok "\n" =~ /./, 'use re "/s"';
no re '/s';
ok "\n" !~ /./, 'no re "/s"';
use re '/m';
ok "\nfoo" =~ /^foo/, 'use re "/m"';
no re '/m';
ok "\nfoo" !~ /^foo/, 'no re "/m"';

use re '/xism';
ok qr// =~ /(?=.*x)(?=.*i)(?=.*s)(?=.*m)/, 'use re "/multiple"';
no re '/ix';
ok qr// =~ /(?!.*x)(?!.*i)(?=.*s)(?=.*m)/, 'no re "/i" only turns off /ix';
no re '/sm';

{
  use re '/x';
  ok 'frelp' =~ /f r e l p/, "use re '/x' in a lexical scope"
}
ok 'f r e l p' =~ /f r e l p/,
 "use re '/x' turns off when it drops out of scope";

SKIP: {
  if (
      !$Config::Config{d_setlocale}
   || $Config::Config{ccflags} =~ /\bD?NO_LOCALE\b/
  ) {
    skip "no locale support", 7
  }
  use locale;
  use re '/u';
  is qr//, '(?^u:)', 'use re "/u" with active locale';
  no re '/u';
  is qr//, '(?^l:)', 'no re "/u" reverts to /l with locale in scope';
  no re '/l';
  is qr//, '(?^l:)', 'no re "/l" is a no-op with locale in scope';
  use re '/d';
  is qr//, '(?^:)', 'use re "/d" with locale in scope';
  no re '/l';
  no re '/u';
  is qr//, '(?^:)',
    'no re "/l" and "/u" are no-ops when not on (locale scope)';
  no re "/d";
  is qr//, '(?^l:)', 'no re "/d" reverts to /l with locale in scope';
  use re "/u";
  no re "/d";
  is qr//, '(?^u:)', 'no re "/d" is a no-op when not on (locale scope)';
}

{
  use feature "unicode_strings";
  use re '/d';
  is qr//, '(?^:)', 'use re "/d" in Unicode scope';
  no re '/d';
  is qr//, '(?^u:)', 'no re "/d" reverts to /u in Unicode scope';
  no re '/u';
  is qr//, '(?^u:)', 'no re "/u" is a no-op in Unicode scope';
  no re '/d';
  is qr//, '(?^u:)', 'no re "/d" is a no-op when not on';
  use re '/u';
  no feature 'unicode_strings';
  is qr//, '(?^u:)', 'use re "/u" is not tied to unicode_strings feature';
}

use re '/u';
is qr//, '(?^u:)', 'use re "/u"';
no re '/u';
is qr//, '(?^:)', 'no re "/u" reverts to /d';
no re '/u';
is qr//, '(?^:)', 'no re "/u" is a no-op when not on';
no re '/d';
is qr//, '(?^:)', 'no re "/d" is a no-op when not on';

{
  local $SIG{__WARN__} = sub {
   ok $_[0] =~ /Unknown regular expression flag "\x{100}"/,
       "warning with unknown regexp flags in use re '/flags'"
  };
  import re "/\x{100}"
}

# use re '/flags' in combination with explicit flags
use re '/xi';
ok "A\n\n" =~ / a.$/sm, 'use re "/xi" in combination with explicit /sm';
{
  local $::TODO = "test requires perl 5.16 syntax";
  # (remove the evals, the quotes, and the ‘no warnings’ when removing the
  # to-do notice)
  no warnings;
  use re '/u';
  is eval 'qr//d', '(?^:)', 'explicit /d in re "/u" scope';
  use re '/d';
  is eval 'qr//u', '(?^u:)', 'explicit /u in re "/d" scope';
}
no re '/x';

# Verify one and two a's work
use re '/ia';
is qr//, '(?^ai:)', 'use re "/ia"';
no re '/ia';
is qr//, '(?^:)', 'no re "/ia"';
use re '/aai';
is qr//, '(?^aai:)', 'use re "/aai"';
no re '/aai';
is qr//, '(?^:)', 'no re "/aai"';

# use re "/adul" combinations
{
  my $w;
  local $SIG{__WARN__} = sub { $w = shift };
  for my $i (@flags) {
    for my $j (@flags) {
      $w = "";
      eval "use re '/$i$j'";
      if ($i eq $j) {
        if ($i eq 'a') {
          is ($w, "", "no warning with use re \"/aa\", $w");
        }
        else {
            like $w, qr/The \"$i\" flag may not appear twice/,
              "warning with use re \"/$i$i\"";
        }
      }
      else {
        if ($j =~ /$i/) {
          # If one is a subset of the other, re.pm uses the longest one.
          like $w, qr/The "$j" and "$i" flags are exclusive/,
            "warning with eval \"use re \"/$j$i\"";
        }
        else {
          like $w, qr/The "$i" and "$j" flags are exclusive/,
            "warning with eval \"use re \"/$i$j\"";
        }
      }
    }
  }

  $w = "";
  eval "use re '/axaa'";
  like $w, qr/The "a" flag may only appear a maximum of twice/,
    "warning with eval \"use re \"/axaa\"";


}
