#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 22;

@x = (1, 2, 3);
is( join(':',@x), '1:2:3', 'join an array with character');

is( join('',1,2,3), '123', 'join list with no separator');

is( join(':',split(/ /,"1 2 3")), '1:2:3', 'join implicit array with character');

my $f = 'a';
$f = join ',', 'b', $f, 'e';
is( $f, 'b,a,e', 'join list back to self, middle of list');

$f = 'a';
$f = join ',', $f, 'b', 'e';
is( $f, 'a,b,e', 'join list back to self, beginning of list');

$f = 'a';
$f = join $f, 'b', 'e', 'k';
is( $f, 'baeak', 'join back to self, self is join character');

# 7,8 check for multiple read of tied objects
{ package X;
  sub TIESCALAR { my $x = 7; bless \$x };
  sub FETCH { my $y = shift; $$y += 5 };
  tie my $t, 'X';
  my $r = join ':', $t, 99, $t, 99;
  main::is($r, '12:99:17:99', 'check for multiple read of tied objects, with separator');
  $r = join '', $t, 99, $t, 99;
  main::is($r, '22992799', 'check for multiple read of tied objects, w/o separator, and magic');
};

# 9,10 and for multiple read of undef
{ my $s = 5;
  local ($^W, $SIG{__WARN__}) = ( 1, sub { $s+=4 } );
  my $r = join ':', 'a', undef, $s, 'b', undef, $s, 'c';
  is( $r, 'a::9:b::13:c', 'multiple read of undef, with separator');
  my $r = join '', 'a', undef, $s, 'b', undef, $s, 'c';
  is( $r, 'a17b21c', '... and without separator');
};

{ my $s = join("", chr(0x1234), chr(0xff));
  is( $s, "\x{1234}\x{ff}", 'join two characters with multiple bytes, get two characters');
}

{ my $s = join(chr(0xff), chr(0x1234), "");
  is( $s, "\x{1234}\x{ff}", 'high byte character as separator, 1 multi-byte character in front');
}

{ my $s = join(chr(0x1234), chr(0xff), chr(0x2345));
  is( $s, "\x{ff}\x{1234}\x{2345}", 'multibyte character as separator');
}

{ my $s = join(chr(0xff), chr(0x1234), chr(0xfe));
  is( $s, "\x{1234}\x{ff}\x{fe}", 'high byte as separator, multi-byte and high byte list');
}

{ # [perl #24846] $jb2 should be in bytes, not in utf8.
  my $b = "abc\304";
  my $u = "abc\x{0100}";

  sub join_into_my_variable {
    my $r = join("", @_);
    return $r;
  }

  sub byte_is {
    use bytes;
    return $_[0] eq $_[1] ? pass($_[2]) : fail($_[2]);
  }

  my $jb1 = join_into_my_variable("", $b);
  my $ju1 = join_into_my_variable("", $u);
  my $jb2 = join_into_my_variable("", $b);
  my $ju2 = join_into_my_variable("", $u);

  note( 'utf8 and byte checks, perl #24846' );

  byte_is($jb1, $b);
  is( $jb1, $b );

  byte_is($ju1, $u);
  is( $ju1, $u );

  byte_is($jb2, $b);
  is( $jb2, $b );

  byte_is($ju2, $u);
  is( $ju2, $u );
}

