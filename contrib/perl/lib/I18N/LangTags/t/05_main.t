
require 5;
 # Time-stamp: "2004-03-30 17:52:14 AST"
use strict;
use Test;
BEGIN { plan tests => 64 };
BEGIN { ok 1 }
use I18N::LangTags (':ALL');

print "# Perl v$], I18N::LangTags v$I18N::LangTags::VERSION\n";

ok !is_language_tag('');
ok  is_language_tag('fr');
ok  is_language_tag('fr-ca');
ok  is_language_tag('fr-CA');
ok !is_language_tag('fr-CA-');
ok !is_language_tag('fr_CA');
ok  is_language_tag('fr-ca-joual');
ok !is_language_tag('frca');
ok  is_language_tag('nav'); # (not actual tag)
ok  is_language_tag('nav-shiprock'); # (not actual tag)
ok !is_language_tag('nav-ceremonial'); # subtag too long
ok !is_language_tag('x');
ok !is_language_tag('i');
ok  is_language_tag('i-borg'); # NB: fictitious tag
ok  is_language_tag('x-borg');
ok  is_language_tag('x-borg-prot5123');
ok  same_language_tag('x-borg-prot5123', 'i-BORG-Prot5123' );
ok !same_language_tag('en', 'en-us' );

ok 0 == similarity_language_tag('en-ca', 'fr-ca');
ok 1 == similarity_language_tag('en-ca', 'en-us');
ok 2 == similarity_language_tag('en-us-southern', 'en-us-western');
ok 2 == similarity_language_tag('en-us-southern', 'en-us');

ok grep $_ eq 'hi', panic_languages('kok');
ok grep $_ eq 'en', panic_languages('x-woozle-wuzzle');
ok ! grep $_ eq 'mr', panic_languages('it');
ok grep $_ eq 'es', panic_languages('it');
ok grep $_ eq 'it', panic_languages('es');


print "# Now the ::List tests...\n";
print "# Perl v$], I18N::LangTags::List v$I18N::LangTags::List::VERSION\n";

use I18N::LangTags::List;
foreach my $lt (qw(
 en
 en-us
 en-kr
 el
 elx
 i-mingo
 i-mingo-tom
 x-mingo-tom
 it
 it-it
 it-IT
 it-FR
 ak
 aka
 jv
 jw
 no
 no-nyn
 nn
 i-lux
 lb
 wa
 yi
 ji
 den-syllabic
 den-syllabic-western
 den-western
 den-latin
 cre-syllabic
 cre-syllabic-western
 cre-western
 cre-latin
 cr-syllabic
 cr-syllabic-western
 cr-western
 cr-latin
)) {
  my $name = I18N::LangTags::List::name($lt);
  if($name) {
    ok(1);
    print "#        $lt -> $name\n";
  } else {
    ok(0);
    print "#        Failed lookup on $lt\n";
  }
}



print "# So there!\n";

