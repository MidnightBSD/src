#!/usr/bin/perl -w

require 5.002;

$runtests=shift(@ARGV);
if ( -f "t/testfunc.pl" ) {
  require "t/testfunc.pl";
  $dir="./lib";
  $tdir="t";
} elsif ( -f "testfunc.pl" ) {
  require "testfunc.pl";
  $dir="../lib";
  $tdir=".";
} else {
  die "ERROR: cannot find testfunc.pl\n";
}

unshift(@INC,$dir);
use Locale::Language;

%type = ( "LOCALE_CODE_ALPHA_2" => LOCALE_CODE_ALPHA_2,
          "LOCALE_CODE_ALPHA_3" => LOCALE_CODE_ALPHA_3,
          "LOCALE_CODE_TERM"    => LOCALE_CODE_TERM,
        );

sub test {
   my(@test) = @_;
   $test[1]  = $type{$test[1]}
     if (@test == 2  &&  $test[1]  &&  exists $type{$test[1]});
   return code2language(@test);
}

$tests = "

in ~ _undef_

iw ~ _undef_

ji ~ _undef_

jp ~ _undef_

sh ~ _undef_

zz ~ _undef_

~ _undef_

_undef_ ~ _undef_

DA
   ~
   Danish

aa
   ~
   Afar

ae
   ~
   Avestan

bs
   ~
   Bosnian

ce
   ~
   Chechen

ch
   ~
   Chamorro

cu
   ~
   Church Slavic

cv
   ~
   Chuvash

en
   ~
   English

eo
   ~
   Esperanto

fi
   ~
   Finnish

gv
   ~
   Manx

he
   ~
   Hebrew

ho
   ~
   Hiri Motu

hz
   ~
   Herero

id
   ~
   Indonesian

iu
   ~
   Inuktitut

ki
   ~
   Kikuyu

kj
   ~
   Kuanyama

kv
   ~
   Komi

kw
   ~
   Cornish

lb
   ~
   Luxembourgish

mh
   ~
   Marshallese

nb
   ~
   Bokmal, Norwegian

nd
   ~
   Ndebele, North

ng
   ~
   Ndonga

nn
   ~
   Norwegian Nynorsk

nr
   ~
   Ndebele, South

nv
   ~
   Navajo

ny
   ~
   Chichewa

oc
   ~
   Occitan (post 1500)

os
   ~
   Ossetian

pi
   ~
   Pali

sc
   ~
   Sardinian

se
   ~
   Northern Sami

ug
   ~
   Uighur

yi
   ~
   Yiddish

za
   ~
   Zhuang

zu
   ~
   Zulu

";

print "code2language...\n";
test_Func(\&test,$tests,$runtests);

1;
# Local Variables:
# mode: cperl
# indent-tabs-mode: nil
# cperl-indent-level: 3
# cperl-continued-statement-offset: 2
# cperl-continued-brace-offset: 0
# cperl-brace-offset: 0
# cperl-brace-imaginary-offset: 0
# cperl-label-offset: -2
# End:
