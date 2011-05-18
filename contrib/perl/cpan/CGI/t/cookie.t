#!perl -w

use strict;

# to have a consistent baseline, we nail the current time
# to 100 seconds after the epoch
BEGIN {
    *CORE::GLOBAL::time = sub { 100 };
}

use Test::More 'no_plan';
use CGI::Util qw(escape unescape);
use POSIX qw(strftime);
use CGI::Cookie;

#-----------------------------------------------------------------------------
# make sure module loaded
#-----------------------------------------------------------------------------

my @test_cookie = (
           # including leading and trailing whitespace in first cookie
           ' foo=123 ; bar=qwerty; baz=wibble; qux=a1',
           'foo=123; bar=qwerty; baz=wibble;',
           'foo=vixen; bar=cow; baz=bitch; qux=politician',
           'foo=a%20phrase; bar=yes%2C%20a%20phrase; baz=%5Ewibble; qux=%27',
           );

#-----------------------------------------------------------------------------
# Test parse
#-----------------------------------------------------------------------------

{
  my $result = CGI::Cookie->parse($test_cookie[0]);
  is(ref($result), 'HASH', "Hash ref returned in scalar context");

  my @result = CGI::Cookie->parse($test_cookie[0]);
  is(@result, 8, "returns correct number of fields");

  @result = CGI::Cookie->parse($test_cookie[1]);
  is(@result, 6, "returns correct number of fields");

  my %result = CGI::Cookie->parse($test_cookie[0]);
  is($result{foo}->value, '123', "cookie foo is correct");
  is($result{bar}->value, 'qwerty', "cookie bar is correct");
  is($result{baz}->value, 'wibble', "cookie baz is correct");
  is($result{qux}->value, 'a1', "cookie qux is correct");

  my @array   = CGI::Cookie->parse('');
  my $scalar  = CGI::Cookie->parse('');
  is_deeply(\@array, [], " parse('') returns an empty array   in list context   (undocumented)");
  is_deeply($scalar, {}, " parse('') returns an empty hashref in scalar context (undocumented)");

  @array   = CGI::Cookie->parse(undef);
  $scalar  = CGI::Cookie->parse(undef);
  is_deeply(\@array, [], " parse(undef) returns an empty array   in list context   (undocumented)");
  is_deeply($scalar, {}, " parse(undef) returns an empty hashref in scalar context (undocumented)");
}

#-----------------------------------------------------------------------------
# Test fetch
#-----------------------------------------------------------------------------

{
  # make sure there are no cookies in the environment
  delete $ENV{HTTP_COOKIE};
  delete $ENV{COOKIE};

  my %result = CGI::Cookie->fetch();
  ok(keys %result == 0, "No cookies in environment, returns empty list");

  # now set a cookie in the environment and try again
  $ENV{HTTP_COOKIE} = $test_cookie[2];
  %result = CGI::Cookie->fetch();
  ok(eq_set([keys %result], [qw(foo bar baz qux)]),
     "expected cookies extracted");

  is(ref($result{foo}), 'CGI::Cookie', 'Type of objects returned is correct');
  is($result{foo}->value, 'vixen',      "cookie foo is correct");
  is($result{bar}->value, 'cow',        "cookie bar is correct");
  is($result{baz}->value, 'bitch',      "cookie baz is correct");
  is($result{qux}->value, 'politician', "cookie qux is correct");

  # Delete that and make sure it goes away
  delete $ENV{HTTP_COOKIE};
  %result = CGI::Cookie->fetch();
  ok(keys %result == 0, "No cookies in environment, returns empty list");

  # try another cookie in the other environment variable thats supposed to work
  $ENV{COOKIE} = $test_cookie[3];
  %result = CGI::Cookie->fetch();
  ok(eq_set([keys %result], [qw(foo bar baz qux)]),
     "expected cookies extracted");

  is(ref($result{foo}), 'CGI::Cookie', 'Type of objects returned is correct');
  is($result{foo}->value, 'a phrase', "cookie foo is correct");
  is($result{bar}->value, 'yes, a phrase', "cookie bar is correct");
  is($result{baz}->value, '^wibble', "cookie baz is correct");
  is($result{qux}->value, "'", "cookie qux is correct");
}

#-----------------------------------------------------------------------------
# Test raw_fetch
#-----------------------------------------------------------------------------

{
  # make sure there are no cookies in the environment
  delete $ENV{HTTP_COOKIE};
  delete $ENV{COOKIE};

  my %result = CGI::Cookie->raw_fetch();
  ok(keys %result == 0, "No cookies in environment, returns empty list");

  # now set a cookie in the environment and try again
  $ENV{HTTP_COOKIE} = $test_cookie[2];
  %result = CGI::Cookie->raw_fetch();
  ok(eq_set([keys %result], [qw(foo bar baz qux)]),
     "expected cookies extracted");

  is(ref($result{foo}), '', 'Plain scalar returned');
  is($result{foo}, 'vixen',      "cookie foo is correct");
  is($result{bar}, 'cow',        "cookie bar is correct");
  is($result{baz}, 'bitch',      "cookie baz is correct");
  is($result{qux}, 'politician', "cookie qux is correct");

  # Delete that and make sure it goes away
  delete $ENV{HTTP_COOKIE};
  %result = CGI::Cookie->raw_fetch();
  ok(keys %result == 0, "No cookies in environment, returns empty list");

  # try another cookie in the other environment variable thats supposed to work
  $ENV{COOKIE} = $test_cookie[3];
  %result = CGI::Cookie->raw_fetch();
  ok(eq_set([keys %result], [qw(foo bar baz qux)]),
     "expected cookies extracted");

  is(ref($result{foo}), '', 'Plain scalar returned');
  is($result{foo}, 'a%20phrase', "cookie foo is correct");
  is($result{bar}, 'yes%2C%20a%20phrase', "cookie bar is correct");
  is($result{baz}, '%5Ewibble', "cookie baz is correct");
  is($result{qux}, '%27', "cookie qux is correct");

  $ENV{COOKIE} = '$Version=1; foo; $Path="/test"';
  %result = CGI::Cookie->raw_fetch();
  is($result{foo}, '', 'no value translates to empty string');
}

#-----------------------------------------------------------------------------
# Test new
#-----------------------------------------------------------------------------

{
  # Try new with full information provided
  my $c = CGI::Cookie->new(-name    => 'foo',
               -value   => 'bar',
               -expires => '+3M',
               -domain  => '.capricorn.com',
               -path    => '/cgi-bin/database',
               -secure  => 1,
               -httponly=> 1
              );
  is(ref($c), 'CGI::Cookie', 'new returns objects of correct type');
  is($c->name   , 'foo',               'name is correct');
  is($c->value  , 'bar',               'value is correct');
  like($c->expires, '/^[a-z]{3},\s*\d{2}-[a-z]{3}-\d{4}/i', 'expires in correct format');
  is($c->domain , '.capricorn.com',    'domain is correct');
  is($c->path   , '/cgi-bin/database', 'path is correct');
  ok($c->secure , 'secure attribute is set');
  ok( $c->httponly, 'httponly attribute is set' );

  # now try it with the only two manditory values (should also set the default path)
  $c = CGI::Cookie->new(-name    =>  'baz',
            -value   =>  'qux',
               );
  is(ref($c), 'CGI::Cookie', 'new returns objects of correct type');
  is($c->name   , 'baz', 'name is correct');
  is($c->value  , 'qux', 'value is correct');
  ok(!defined $c->expires,       'expires is not set');
  ok(!defined $c->domain ,       'domain attributeis not set');
  is($c->path, '/',      'path atribute is set to default');
  ok(!defined $c->secure ,       'secure attribute is set');
  ok( !defined $c->httponly, 'httponly attribute is not set' );

# I'm really not happy about the restults of this section.  You pass
# the new method invalid arguments and it just merilly creates a
# broken object :-)
# I've commented them out because they currently pass but I don't
# think they should.  I think this is testing broken behaviour :-(

#    # This shouldn't work
#    $c = CGI::Cookie->new(-name => 'baz' );
#
#    is(ref($c), 'CGI::Cookie', 'new returns objects of correct type');
#    is($c->name   , 'baz',     'name is correct');
#    ok(!defined $c->value, "Value is undefined ");
#    ok(!defined $c->expires, 'expires is not set');
#    ok(!defined $c->domain , 'domain attributeis not set');
#    is($c->path   , '/', 'path atribute is set to default');
#    ok(!defined $c->secure , 'secure attribute is set');

}

#-----------------------------------------------------------------------------
# Test as_string
#-----------------------------------------------------------------------------

{
  my $c = CGI::Cookie->new(-name    => 'Jam',
               -value   => 'Hamster',
               -expires => '+3M',
               -domain  => '.pie-shop.com',
               -path    => '/',
               -secure  => 1,
               -httponly=> 1
              );

  my $name = $c->name;
  like($c->as_string, "/$name/", "Stringified cookie contains name");

  my $value = $c->value;
  like($c->as_string, "/$value/", "Stringified cookie contains value");

  my $expires = $c->expires;
  like($c->as_string, "/$expires/", "Stringified cookie contains expires");

  my $domain = $c->domain;
  like($c->as_string, "/$domain/", "Stringified cookie contains domain");

  my $path = $c->path;
  like($c->as_string, "/$path/", "Stringified cookie contains path");

  like($c->as_string, '/secure/', "Stringified cookie contains secure");

  like( $c->as_string, '/HttpOnly/',
    "Stringified cookie contains HttpOnly" );

  $c = CGI::Cookie->new(-name    =>  'Hamster-Jam',
            -value   =>  'Tulip',
               );

  $name = $c->name;
  like($c->as_string, "/$name/", "Stringified cookie contains name");

  $value = $c->value;
  like($c->as_string, "/$value/", "Stringified cookie contains value");

  ok($c->as_string !~ /expires/, "Stringified cookie has no expires field");

  ok($c->as_string !~ /domain/, "Stringified cookie has no domain field");

  $path = $c->path;
  like($c->as_string, "/$path/", "Stringified cookie contains path");

  ok($c->as_string !~ /secure/, "Stringified cookie does not contain secure");

  ok( $c->as_string !~ /HttpOnly/,
    "Stringified cookie does not contain HttpOnly" );
}

#-----------------------------------------------------------------------------
# Test compare
#-----------------------------------------------------------------------------

{
  my $c1 = CGI::Cookie->new(-name    => 'Jam',
                -value   => 'Hamster',
                -expires => '+3M',
                -domain  => '.pie-shop.com',
                -path    => '/',
                -secure  => 1
               );

  # have to use $c1->expires because the time will occasionally be
  # different between the two creates causing spurious failures.
  my $c2 = CGI::Cookie->new(-name    => 'Jam',
                -value   => 'Hamster',
                -expires => $c1->expires,
                -domain  => '.pie-shop.com',
                -path    => '/',
                -secure  => 1
               );

  # This looks titally whacked, but it does the -1, 0, 1 comparison
  # thing so 0 means they match
  is($c1->compare("$c1"), 0, "Cookies are identical");
  is( "$c1", "$c2", "Cookies are identical");

  $c1 = CGI::Cookie->new(-name   => 'Jam',
             -value  => 'Hamster',
             -domain => '.foo.bar.com'
            );

  # have to use $c1->expires because the time will occasionally be
  # different between the two creates causing spurious failures.
  $c2 = CGI::Cookie->new(-name    =>  'Jam',
             -value   =>  'Hamster',
            );

  # This looks titally whacked, but it does the -1, 0, 1 comparison
  # thing so 0 (i.e. false) means they match
  is($c1->compare("$c1"), 0, "Cookies are identical");
  ok($c1->compare("$c2"), "Cookies are not identical");

  $c2->domain('.foo.bar.com');
  is($c1->compare("$c2"), 0, "Cookies are identical");
}

#-----------------------------------------------------------------------------
# Test name, value, domain, secure, expires and path
#-----------------------------------------------------------------------------

{
  my $c = CGI::Cookie->new(-name    => 'Jam',
               -value   => 'Hamster',
               -expires => '+3M',
               -domain  => '.pie-shop.com',
               -path    => '/',
               -secure  => 1
               );

  is($c->name,          'Jam',   'name is correct');
  is($c->name('Clash'), 'Clash', 'name is set correctly');
  is($c->name,          'Clash', 'name now returns updated value');

  # this is insane!  it returns a simple scalar but can't accept one as
  # an argument, you have to give it an arrary ref.  It's totally
  # inconsitent with these other methods :-(
  is($c->value,           'Hamster', 'value is correct');
  is($c->value(['Gerbil']), 'Gerbil',  'value is set correctly');
  is($c->value,           'Gerbil',  'value now returns updated value');

  my $exp = $c->expires;
  like($c->expires,         '/^[a-z]{3},\s*\d{2}-[a-z]{3}-\d{4}/i', 'expires is correct');
  like($c->expires('+12h'), '/^[a-z]{3},\s*\d{2}-[a-z]{3}-\d{4}/i', 'expires is set correctly');
  like($c->expires,         '/^[a-z]{3},\s*\d{2}-[a-z]{3}-\d{4}/i', 'expires now returns updated value');
  isnt($c->expires, $exp, "Expiry time has changed");

  is($c->domain,                  '.pie-shop.com', 'domain is correct');
  is($c->domain('.wibble.co.uk'), '.wibble.co.uk', 'domain is set correctly');
  is($c->domain,                  '.wibble.co.uk', 'domain now returns updated value');

  is($c->path,             '/',        'path is correct');
  is($c->path('/basket/'), '/basket/', 'path is set correctly');
  is($c->path,             '/basket/', 'path now returns updated value');

  ok($c->secure,     'secure attribute is set');
  ok(!$c->secure(0), 'secure attribute is cleared');
  ok(!$c->secure,    'secure attribute is cleared');
}

#----------------------------------------------------------------------------
# Max-age
#----------------------------------------------------------------------------

MAX_AGE: {
    my $cookie = CGI::Cookie->new( -name=>'a', value=>'b', '-expires' => 'now',);
    is $cookie->expires, 'Thu, 01-Jan-1970 00:01:40 GMT';
    is $cookie->max_age => undef, 'max-age is undefined when setting expires';

    $cookie = CGI::Cookie->new( -name=>'a', 'value'=>'b' );
    $cookie->max_age( '+4d' );

    is $cookie->expires, undef, 'expires is undef when setting max_age';
    is $cookie->max_age => 4*24*60*60, 'setting via max-age';

    $cookie->max_age( '113' );
    is $cookie->max_age => 13, 'max_age(num) as delta';
}


#----------------------------------------------------------------------------
# bake
#----------------------------------------------------------------------------

BAKE: {
    my $cookie = CGI::Cookie->new( -name=>'a', value=>'b', '-expires' => 'now',);
    eval { $cookie->bake };
    is($@,'', "calling bake() without mod_perl should survive"); 
}

#-----------------------------------------------------------------------------
# Apache2?::Cookie compatibility.
#-----------------------------------------------------------------------------
APACHEREQ: {
    my $r = Apache::Faker->new;
    isa_ok $r, 'Apache';
    ok my $c = CGI::Cookie->new(
        $r,
        -name  => 'Foo',
        -value => 'Bar',
    ), 'Pass an Apache object to the CGI::Cookie constructor';
    isa_ok $c, 'CGI::Cookie';
    ok $c->bake($r), 'Bake the cookie';
    ok eq_array( $r->{check}, [ 'Set-Cookie', $c->as_string ]),
        'bake() should call headers_out->set()';

    $r = Apache2::Faker->new;
    isa_ok $r, 'Apache2::RequestReq';
    ok $c = CGI::Cookie->new(
        $r,
        -name  => 'Foo',
        -value => 'Bar',
    ), 'Pass an Apache::RequestReq object to the CGI::Cookie constructor';
    isa_ok $c, 'CGI::Cookie';
    ok $c->bake($r), 'Bake the cookie';
    ok eq_array( $r->{check}, [ 'Set-Cookie', $c->as_string ]),
        'bake() should call headers_out->set()';
}


package Apache::Faker;
sub new { bless {}, shift }
sub isa {
    my ($self, $pkg) = @_;
    return $pkg eq 'Apache';
}
sub headers_out { shift }
sub add { shift->{check} = \@_; }

package Apache2::Faker;
sub new { bless {}, shift }
sub isa {
    my ($self, $pkg) = @_;
    return $pkg eq 'Apache2::RequestReq';
}
sub headers_out { shift }
sub add { shift->{check} = \@_; }
