#
# t/test.pl - most of Test::More functionality without the fuss, plus
# has mappings native_to_latin1 and latin1_to_native so that fewer tests
# on non ASCII-ish platforms need to be skipped


# NOTE:
#
# Increment ($x++) has a certain amount of cleverness for things like
#
#   $x = 'zz';
#   $x++; # $x eq 'aaa';
#
# stands more chance of breaking than just a simple
#
#   $x = $x + 1
#
# In this file, we use the latter "Baby Perl" approach, and increment
# will be worked over by t/op/inc.t

$Level = 1;
my $test = 1;
my $planned;
my $noplan;
my $Perl;       # Safer version of $^X set by which_perl()

# This defines ASCII/UTF-8 vs EBCDIC/UTF-EBCDIC
$::IS_ASCII  = ord 'A' ==  65;
$::IS_EBCDIC = ord 'A' == 193;

$TODO = 0;
$NO_ENDING = 0;
$Tests_Are_Passing = 1;

# Use this instead of print to avoid interference while testing globals.
sub _print {
    local($\, $", $,) = (undef, ' ', '');
    print STDOUT @_;
}

sub _print_stderr {
    local($\, $", $,) = (undef, ' ', '');
    print STDERR @_;
}

sub plan {
    my $n;
    if (@_ == 1) {
	$n = shift;
	if ($n eq 'no_plan') {
	  undef $n;
	  $noplan = 1;
	}
    } else {
	my %plan = @_;
	$n = $plan{tests};
    }
    _print "1..$n\n" unless $noplan;
    $planned = $n;
}


# Set the plan at the end.  See Test::More::done_testing.
sub done_testing {
    my $n = $test - 1;
    $n = shift if @_;

    _print "1..$n\n";
    $planned = $n;
}


END {
    my $ran = $test - 1;
    if (!$NO_ENDING) {
	if (defined $planned && $planned != $ran) {
	    _print_stderr
		"# Looks like you planned $planned tests but ran $ran.\n";
	} elsif ($noplan) {
	    _print "1..$ran\n";
	}
    }
}

sub _diag {
    return unless @_;
    my @mess = _comment(@_);
    $TODO ? _print(@mess) : _print_stderr(@mess);
}

# Use this instead of "print STDERR" when outputting failure diagnostic
# messages
sub diag {
    _diag(@_);
}

# Use this instead of "print" when outputting informational messages
sub note {
    return unless @_;
    _print( _comment(@_) );
}

sub is_miniperl {
    return !defined &DynaLoader::boot_DynaLoader;
}

sub _comment {
    return map { /^#/ ? "$_\n" : "# $_\n" }
           map { split /\n/ } @_;
}

sub skip_all {
    if (@_) {
        _print "1..0 # Skip @_\n";
    } else {
	_print "1..0\n";
    }
    exit(0);
}

sub skip_all_if_miniperl {
    skip_all(@_) if is_miniperl();
}

sub skip_all_without_dynamic_extension {
    my $extension = shift;
    skip_all("no dynamic loading on miniperl, no $extension") if is_miniperl();
    unless (eval {require Config; 1}) {
	warn "test.pl had problems loading Config: $@";
	return;
    }
    $extension =~ s!::!/!g;
    return if ($Config::Config{extensions} =~ /\b$extension\b/);
    skip_all("$extension was not built");
}

sub skip_all_without_perlio {
    skip_all('no PerlIO') unless PerlIO::Layer->find('perlio');
}

sub skip_all_without_config {
    unless (eval {require Config; 1}) {
	warn "test.pl had problems loading Config: $@";
	return;
    }
    foreach (@_) {
	next if $Config::Config{$_};
	my $key = $_; # Need to copy, before trying to modify.
	$key =~ s/^use//;
	$key =~ s/^d_//;
	skip_all("no $key");
    }
}

sub _ok {
    my ($pass, $where, $name, @mess) = @_;
    # Do not try to microoptimize by factoring out the "not ".
    # VMS will avenge.
    my $out;
    if ($name) {
        # escape out '#' or it will interfere with '# skip' and such
        $name =~ s/#/\\#/g;
	$out = $pass ? "ok $test - $name" : "not ok $test - $name";
    } else {
	$out = $pass ? "ok $test" : "not ok $test";
    }

    if ($TODO) {
	$out = $out . " # TODO $TODO";
    } else {
	$Tests_Are_Passing = 0 unless $pass;
    }

    _print "$out\n";

    if ($pass) {
	note @mess; # Ensure that the message is properly escaped.
    }
    else {
	_diag "# Failed $where\n";
	_diag @mess;
    }

    $test = $test + 1; # don't use ++

    return $pass;
}

sub _where {
    my @caller = caller($Level);
    return "at $caller[1] line $caller[2]";
}

# DON'T use this for matches. Use like() instead.
sub ok ($@) {
    my ($pass, $name, @mess) = @_;
    _ok($pass, _where(), $name, @mess);
}

sub _q {
    my $x = shift;
    return 'undef' unless defined $x;
    my $q = $x;
    $q =~ s/\\/\\\\/g;
    $q =~ s/'/\\'/g;
    return "'$q'";
}

sub _qq {
    my $x = shift;
    return defined $x ? '"' . display ($x) . '"' : 'undef';
};

# keys are the codes \n etc map to, values are 2 char strings such as \n
my %backslash_escape;
foreach my $x (split //, 'nrtfa\\\'"') {
    $backslash_escape{ord eval "\"\\$x\""} = "\\$x";
}
# A way to display scalars containing control characters and Unicode.
# Trying to avoid setting $_, or relying on local $_ to work.
sub display {
    my @result;
    foreach my $x (@_) {
        if (defined $x and not ref $x) {
            my $y = '';
            foreach my $c (unpack("U*", $x)) {
                if ($c > 255) {
                    $y = $y . sprintf "\\x{%x}", $c;
                } elsif ($backslash_escape{$c}) {
                    $y = $y . $backslash_escape{$c};
                } else {
                    my $z = chr $c; # Maybe we can get away with a literal...
                    if ($z =~ /[[:^print:]]/) {

                        # Use octal for characters traditionally expressed as
                        # such: the low controls
                        if ($c <= 037) {
                            $z = sprintf "\\%03o", $c;
                        } else {
                            $z = sprintf "\\x{%x}", $c;
                        }
                    }
                    $y = $y . $z;
                }
            }
            $x = $y;
        }
        return $x unless wantarray;
        push @result, $x;
    }
    return @result;
}

sub is ($$@) {
    my ($got, $expected, $name, @mess) = @_;

    my $pass;
    if( !defined $got || !defined $expected ) {
        # undef only matches undef
        $pass = !defined $got && !defined $expected;
    }
    else {
        $pass = $got eq $expected;
    }

    unless ($pass) {
	unshift(@mess, "#      got "._qq($got)."\n",
		       "# expected "._qq($expected)."\n");
    }
    _ok($pass, _where(), $name, @mess);
}

sub isnt ($$@) {
    my ($got, $isnt, $name, @mess) = @_;

    my $pass;
    if( !defined $got || !defined $isnt ) {
        # undef only matches undef
        $pass = defined $got || defined $isnt;
    }
    else {
        $pass = $got ne $isnt;
    }

    unless( $pass ) {
        unshift(@mess, "# it should not be "._qq($got)."\n",
                       "# but it is.\n");
    }
    _ok($pass, _where(), $name, @mess);
}

sub cmp_ok ($$$@) {
    my($got, $type, $expected, $name, @mess) = @_;

    my $pass;
    {
        local $^W = 0;
        local($@,$!);   # don't interfere with $@
                        # eval() sometimes resets $!
        $pass = eval "\$got $type \$expected";
    }
    unless ($pass) {
        # It seems Irix long doubles can have 2147483648 and 2147483648
        # that stringify to the same thing but are actually numerically
        # different. Display the numbers if $type isn't a string operator,
        # and the numbers are stringwise the same.
        # (all string operators have alphabetic names, so tr/a-z// is true)
        # This will also show numbers for some unneeded cases, but will
        # definitely be helpful for things such as == and <= that fail
        if ($got eq $expected and $type !~ tr/a-z//) {
            unshift @mess, "# $got - $expected = " . ($got - $expected) . "\n";
        }
        unshift(@mess, "#      got "._qq($got)."\n",
                       "# expected $type "._qq($expected)."\n");
    }
    _ok($pass, _where(), $name, @mess);
}

# Check that $got is within $range of $expected
# if $range is 0, then check it's exact
# else if $expected is 0, then $range is an absolute value
# otherwise $range is a fractional error.
# Here $range must be numeric, >= 0
# Non numeric ranges might be a useful future extension. (eg %)
sub within ($$$@) {
    my ($got, $expected, $range, $name, @mess) = @_;
    my $pass;
    if (!defined $got or !defined $expected or !defined $range) {
        # This is a fail, but doesn't need extra diagnostics
    } elsif ($got !~ tr/0-9// or $expected !~ tr/0-9// or $range !~ tr/0-9//) {
        # This is a fail
        unshift @mess, "# got, expected and range must be numeric\n";
    } elsif ($range < 0) {
        # This is also a fail
        unshift @mess, "# range must not be negative\n";
    } elsif ($range == 0) {
        # Within 0 is ==
        $pass = $got == $expected;
    } elsif ($expected == 0) {
        # If expected is 0, treat range as absolute
        $pass = ($got <= $range) && ($got >= - $range);
    } else {
        my $diff = $got - $expected;
        $pass = abs ($diff / $expected) < $range;
    }
    unless ($pass) {
        if ($got eq $expected) {
            unshift @mess, "# $got - $expected = " . ($got - $expected) . "\n";
        }
	unshift@mess, "#      got "._qq($got)."\n",
		      "# expected "._qq($expected)." (within "._qq($range).")\n";
    }
    _ok($pass, _where(), $name, @mess);
}

# Note: this isn't quite as fancy as Test::More::like().

sub like   ($$@) { like_yn (0,@_) }; # 0 for -
sub unlike ($$@) { like_yn (1,@_) }; # 1 for un-

sub like_yn ($$$@) {
    my ($flip, undef, $expected, $name, @mess) = @_;
    my $pass;
    $pass = $_[1] =~ /$expected/ if !$flip;
    $pass = $_[1] !~ /$expected/ if $flip;
    unless ($pass) {
	unshift(@mess, "#      got '$_[1]'\n",
		$flip
		? "# expected !~ /$expected/\n" : "# expected /$expected/\n");
    }
    local $Level = $Level + 1;
    _ok($pass, _where(), $name, @mess);
}

sub pass {
    _ok(1, '', @_);
}

sub fail {
    _ok(0, _where(), @_);
}

sub curr_test {
    $test = shift if @_;
    return $test;
}

sub next_test {
  my $retval = $test;
  $test = $test + 1; # don't use ++
  $retval;
}

# Note: can't pass multipart messages since we try to
# be compatible with Test::More::skip().
sub skip {
    my $why = shift;
    my $n    = @_ ? shift : 1;
    for (1..$n) {
        _print "ok $test # skip $why\n";
        $test = $test + 1;
    }
    local $^W = 0;
    last SKIP;
}

sub skip_if_miniperl {
    skip(@_) if is_miniperl();
}

sub todo_skip {
    my $why = shift;
    my $n   = @_ ? shift : 1;

    for (1..$n) {
        _print "not ok $test # TODO & SKIP $why\n";
        $test = $test + 1;
    }
    local $^W = 0;
    last TODO;
}

sub eq_array {
    my ($ra, $rb) = @_;
    return 0 unless $#$ra == $#$rb;
    for my $i (0..$#$ra) {
	next     if !defined $ra->[$i] && !defined $rb->[$i];
	return 0 if !defined $ra->[$i];
	return 0 if !defined $rb->[$i];
	return 0 unless $ra->[$i] eq $rb->[$i];
    }
    return 1;
}

sub eq_hash {
  my ($orig, $suspect) = @_;
  my $fail;
  while (my ($key, $value) = each %$suspect) {
    # Force a hash recompute if this perl's internals can cache the hash key.
    $key = "" . $key;
    if (exists $orig->{$key}) {
      if ($orig->{$key} ne $value) {
        _print "# key ", _qq($key), " was ", _qq($orig->{$key}),
                     " now ", _qq($value), "\n";
        $fail = 1;
      }
    } else {
      _print "# key ", _qq($key), " is ", _qq($value),
                   ", not in original.\n";
      $fail = 1;
    }
  }
  foreach (keys %$orig) {
    # Force a hash recompute if this perl's internals can cache the hash key.
    $_ = "" . $_;
    next if (exists $suspect->{$_});
    _print "# key ", _qq($_), " was ", _qq($orig->{$_}), " now missing.\n";
    $fail = 1;
  }
  !$fail;
}

# We only provide a subset of the Test::More functionality.
sub require_ok ($) {
    my ($require) = @_;
    if ($require =~ tr/[A-Za-z0-9:.]//c) {
	fail("Invalid character in \"$require\", passed to require_ok");
    } else {
	eval <<REQUIRE_OK;
require $require;
REQUIRE_OK
	is($@, '', _where(), "require $require");
    }
}

sub use_ok ($) {
    my ($use) = @_;
    if ($use =~ tr/[A-Za-z0-9:.]//c) {
	fail("Invalid character in \"$use\", passed to use");
    } else {
	eval <<USE_OK;
use $use;
USE_OK
	is($@, '', _where(), "use $use");
    }
}

# runperl - Runs a separate perl interpreter.
# Arguments :
#   switches => [ command-line switches ]
#   nolib    => 1 # don't use -I../lib (included by default)
#   non_portable => Don't warn if a one liner contains quotes
#   prog     => one-liner (avoid quotes)
#   progs    => [ multi-liner (avoid quotes) ]
#   progfile => perl script
#   stdin    => string to feed the stdin
#   stderr   => redirect stderr to stdout
#   args     => [ command-line arguments to the perl program ]
#   verbose  => print the command line

my $is_mswin    = $^O eq 'MSWin32';
my $is_netware  = $^O eq 'NetWare';
my $is_vms      = $^O eq 'VMS';
my $is_cygwin   = $^O eq 'cygwin';

sub _quote_args {
    my ($runperl, $args) = @_;

    foreach (@$args) {
	# In VMS protect with doublequotes because otherwise
	# DCL will lowercase -- unless already doublequoted.
       $_ = q(").$_.q(") if $is_vms && !/^\"/ && length($_) > 0;
       $runperl = $runperl . ' ' . $_;
    }
    return $runperl;
}

sub _create_runperl { # Create the string to qx in runperl().
    my %args = @_;
    my $runperl = which_perl();
    if ($runperl =~ m/\s/) {
        $runperl = qq{"$runperl"};
    }
    #- this allows, for example, to set PERL_RUNPERL_DEBUG=/usr/bin/valgrind
    if ($ENV{PERL_RUNPERL_DEBUG}) {
	$runperl = "$ENV{PERL_RUNPERL_DEBUG} $runperl";
    }
    unless ($args{nolib}) {
	$runperl = $runperl . ' "-I../lib"'; # doublequotes because of VMS
    }
    if ($args{switches}) {
	local $Level = 2;
	die "test.pl:runperl(): 'switches' must be an ARRAYREF " . _where()
	    unless ref $args{switches} eq "ARRAY";
	$runperl = _quote_args($runperl, $args{switches});
    }
    if (defined $args{prog}) {
	die "test.pl:runperl(): both 'prog' and 'progs' cannot be used " . _where()
	    if defined $args{progs};
        $args{progs} = [$args{prog}]
    }
    if (defined $args{progs}) {
	die "test.pl:runperl(): 'progs' must be an ARRAYREF " . _where()
	    unless ref $args{progs} eq "ARRAY";
        foreach my $prog (@{$args{progs}}) {
	    if ($prog =~ tr/'"// && !$args{non_portable}) {
		warn "quotes in prog >>$prog<< are not portable";
	    }
            if ($is_mswin || $is_netware || $is_vms) {
                $runperl = $runperl . qq ( -e "$prog" );
            }
            else {
                $runperl = $runperl . qq ( -e '$prog' );
            }
        }
    } elsif (defined $args{progfile}) {
	$runperl = $runperl . qq( "$args{progfile}");
    } else {
	# You probably didn't want to be sucking in from the upstream stdin
	die "test.pl:runperl(): none of prog, progs, progfile, args, "
	    . " switches or stdin specified"
	    unless defined $args{args} or defined $args{switches}
		or defined $args{stdin};
    }
    if (defined $args{stdin}) {
	# so we don't try to put literal newlines and crs onto the
	# command line.
	$args{stdin} =~ s/\n/\\n/g;
	$args{stdin} =~ s/\r/\\r/g;

	if ($is_mswin || $is_netware || $is_vms) {
	    $runperl = qq{$Perl -e "print qq(} .
		$args{stdin} . q{)" | } . $runperl;
	}
	else {
	    $runperl = qq{$Perl -e 'print qq(} .
		$args{stdin} . q{)' | } . $runperl;
	}
    }
    if (defined $args{args}) {
	$runperl = _quote_args($runperl, $args{args});
    }
    $runperl = $runperl . ' 2>&1' if $args{stderr};
    if ($args{verbose}) {
	my $runperldisplay = $runperl;
	$runperldisplay =~ s/\n/\n\#/g;
	_print_stderr "# $runperldisplay\n";
    }
    return $runperl;
}

sub runperl {
    die "test.pl:runperl() does not take a hashref"
	if ref $_[0] and ref $_[0] eq 'HASH';
    my $runperl = &_create_runperl;
    my $result;

    my $tainted = ${^TAINT};
    my %args = @_;
    exists $args{switches} && grep m/^-T$/, @{$args{switches}} and $tainted = $tainted + 1;

    if ($tainted) {
	# We will assume that if you're running under -T, you really mean to
	# run a fresh perl, so we'll brute force launder everything for you
	my $sep;

	if (! eval {require Config; 1}) {
	    warn "test.pl had problems loading Config: $@";
	    $sep = ':';
	} else {
	    $sep = $Config::Config{path_sep};
	}

	my @keys = grep {exists $ENV{$_}} qw(CDPATH IFS ENV BASH_ENV);
	local @ENV{@keys} = ();
	# Untaint, plus take out . and empty string:
	local $ENV{'DCL$PATH'} = $1 if $is_vms && exists($ENV{'DCL$PATH'}) && ($ENV{'DCL$PATH'} =~ /(.*)/s);
	$ENV{PATH} =~ /(.*)/s;
	local $ENV{PATH} =
	    join $sep, grep { $_ ne "" and $_ ne "." and -d $_ and
		($is_mswin or $is_vms or !(stat && (stat _)[2]&0022)) }
		    split quotemeta ($sep), $1;
	if ($is_cygwin) {   # Must have /bin under Cygwin
	    if (length $ENV{PATH}) {
		$ENV{PATH} = $ENV{PATH} . $sep;
	    }
	    $ENV{PATH} = $ENV{PATH} . '/bin';
	}
	$runperl =~ /(.*)/s;
	$runperl = $1;

	$result = `$runperl`;
    } else {
	$result = `$runperl`;
    }
    $result =~ s/\n\n/\n/ if $is_vms; # XXX pipes sometimes double these
    return $result;
}

# Nice alias
*run_perl = *run_perl = \&runperl; # shut up "used only once" warning

sub DIE {
    _print_stderr "# @_\n";
    exit 1;
}

# A somewhat safer version of the sometimes wrong $^X.
sub which_perl {
    unless (defined $Perl) {
	$Perl = $^X;

	# VMS should have 'perl' aliased properly
	return $Perl if $is_vms;

	my $exe;
	if (! eval {require Config; 1}) {
	    warn "test.pl had problems loading Config: $@";
	    $exe = '';
	} else {
	    $exe = $Config::Config{_exe};
	}
       $exe = '' unless defined $exe;

	# This doesn't absolutize the path: beware of future chdirs().
	# We could do File::Spec->abs2rel() but that does getcwd()s,
	# which is a bit heavyweight to do here.

	if ($Perl =~ /^perl\Q$exe\E$/i) {
	    my $perl = "perl$exe";
	    if (! eval {require File::Spec; 1}) {
		warn "test.pl had problems loading File::Spec: $@";
		$Perl = "./$perl";
	    } else {
		$Perl = File::Spec->catfile(File::Spec->curdir(), $perl);
	    }
	}

	# Build up the name of the executable file from the name of
	# the command.

	if ($Perl !~ /\Q$exe\E$/i) {
	    $Perl = $Perl . $exe;
	}

	warn "which_perl: cannot find $Perl from $^X" unless -f $Perl;

	# For subcommands to use.
	$ENV{PERLEXE} = $Perl;
    }
    return $Perl;
}

sub unlink_all {
    my $count = 0;
    foreach my $file (@_) {
        1 while unlink $file;
	if( -f $file ){
	    _print_stderr "# Couldn't unlink '$file': $!\n";
	}else{
	    ++$count;
	}
    }
    $count;
}

my %tmpfiles;
END { unlink_all keys %tmpfiles }

# A regexp that matches the tempfile names
$::tempfile_regexp = 'tmp\d+[A-Z][A-Z]?';

# Avoid ++, avoid ranges, avoid split //
my @letters = qw(A B C D E F G H I J K L M N O P Q R S T U V W X Y Z);
sub tempfile {
    my $count = 0;
    do {
	my $temp = $count;
	my $try = "tmp$$";
	do {
	    $try = $try . $letters[$temp % 26];
	    $temp = int ($temp / 26);
	} while $temp;
	# Need to note all the file names we allocated, as a second request may
	# come before the first is created.
	if (!-e $try && !$tmpfiles{$try}) {
	    # We have a winner
	    $tmpfiles{$try} = 1;
	    return $try;
	}
	$count = $count + 1;
    } while $count < 26 * 26;
    die "Can't find temporary file name starting 'tmp$$'";
}

# This is the temporary file for _fresh_perl
my $tmpfile = tempfile();

sub _fresh_perl {
    my($prog, $action, $expect, $runperl_args, $name) = @_;

    # Given the choice of the mis-parsable {}
    # (we want an anon hash, but a borked lexer might think that it's a block)
    # or relying on taking a reference to a lexical
    # (\ might be mis-parsed, and the reference counting on the pad may go
    #  awry)
    # it feels like the least-worse thing is to assume that auto-vivification
    # works. At least, this is only going to be a run-time failure, so won't
    # affect tests using this file but not this function.
    $runperl_args->{progfile} = $tmpfile;
    $runperl_args->{stderr} = 1;

    open TEST, ">$tmpfile" or die "Cannot open $tmpfile: $!";

    # VMS adjustments
    if( $is_vms ) {
        $prog =~ s#/dev/null#NL:#;

        # VMS file locking
        $prog =~ s{if \(-e _ and -f _ and -r _\)}
                  {if (-e _ and -f _)}
    }

    print TEST $prog;
    close TEST or die "Cannot close $tmpfile: $!";

    my $results = runperl(%$runperl_args);
    my $status = $?;

    # Clean up the results into something a bit more predictable.
    $results  =~ s/\n+$//;
    $results =~ s/at\s+$::tempfile_regexp\s+line/at - line/g;
    $results =~ s/of\s+$::tempfile_regexp\s+aborted/of - aborted/g;

    # bison says 'parse error' instead of 'syntax error',
    # various yaccs may or may not capitalize 'syntax'.
    $results =~ s/^(syntax|parse) error/syntax error/mig;

    if ($is_vms) {
        # some tests will trigger VMS messages that won't be expected
        $results =~ s/\n?%[A-Z]+-[SIWEF]-[A-Z]+,.*//;

        # pipes double these sometimes
        $results =~ s/\n\n/\n/g;
    }

    # Use the first line of the program as a name if none was given
    unless( $name ) {
        ($first_line, $name) = $prog =~ /^((.{1,50}).*)/;
        $name = $name . '...' if length $first_line > length $name;
    }

    # Historically this was implemented using a closure, but then that means
    # that the tests for closures avoid using this code. Given that there
    # are exactly two callers, doing exactly two things, the simpler approach
    # feels like a better trade off.
    my $pass;
    if ($action eq 'eq') {
	$pass = is($results, $expect, $name);
    } elsif ($action eq '=~') {
	$pass = like($results, $expect, $name);
    } else {
	die "_fresh_perl can't process action '$action'";
    }
	
    unless ($pass) {
        _diag "# PROG: \n$prog\n";
        _diag "# STATUS: $status\n";
    }

    return $pass;
}

#
# fresh_perl_is
#
# Combination of run_perl() and is().
#

sub fresh_perl_is {
    my($prog, $expected, $runperl_args, $name) = @_;

    # _fresh_perl() is going to clip the trailing newlines off the result.
    # This will make it so the test author doesn't have to know that.
    $expected =~ s/\n+$//;

    local $Level = 2;
    _fresh_perl($prog, 'eq', $expected, $runperl_args, $name);
}

#
# fresh_perl_like
#
# Combination of run_perl() and like().
#

sub fresh_perl_like {
    my($prog, $expected, $runperl_args, $name) = @_;
    local $Level = 2;
    _fresh_perl($prog, '=~', $expected, $runperl_args, $name);
}

# Many tests use the same format in __DATA__ or external files to specify a
# sequence of (fresh) tests to run, extra files they may temporarily need, and
# what the expected output is. So have excatly one copy of the code to run that

sub run_multiple_progs {
    my $up = shift;
    my @prgs;
    if ($up) {
	# The tests in lib run in a temporary subdirectory of t, and always
	# pass in a list of "programs" to run
	@prgs = @_;
    } else {
	# The tests below t run in t and pass in a file handle.
	my $fh = shift;
	local $/;
	@prgs = split "\n########\n", <$fh>;
    }

    my $tmpfile = tempfile();

    for (@prgs){
	unless (/\n/) {
	    print "# From $_\n";
	    next;
	}
	my $switch = "";
	my @temps ;
	my @temp_path;
	if (s/^(\s*-\w+)//) {
	    $switch = $1;
	}
	my ($prog, $expected) = split(/\nEXPECT(?:\n|$)/, $_, 2);

	my %reason;
	foreach my $what (qw(skip todo)) {
	    $prog =~ s/^#\s*\U$what\E\s*(.*)\n//m and $reason{$what} = $1;
	    # If the SKIP reason starts ? then it's taken as a code snippet to
	    # evaluate. This provides the flexibility to have conditional SKIPs
	    if ($reason{$what} && $reason{$what} =~ s/^\?//) {
		my $temp = eval $reason{$what};
		if ($@) {
		    die "# In \U$what\E code reason:\n# $reason{$what}\n$@";
		}
		$reason{$what} = $temp;
	    }
	}

	if ($prog =~ /--FILE--/) {
	    my @files = split(/\n--FILE--\s*([^\s\n]*)\s*\n/, $prog) ;
	    shift @files ;
	    die "Internal error: test $_ didn't split into pairs, got " .
		scalar(@files) . "[" . join("%%%%", @files) ."]\n"
		    if @files % 2;
	    while (@files > 2) {
		my $filename = shift @files;
		my $code = shift @files;
		push @temps, $filename;
		if ($filename =~ m#(.*)/# && $filename !~ m#^\.\./#) {
		    require File::Path;
		    File::Path::mkpath($1);
		    push(@temp_path, $1);
		}
		open my $fh, '>', $filename or die "Cannot open $filename: $!\n";
		print $fh $code;
		close $fh or die "Cannot close $filename: $!\n";
	    }
	    shift @files;
	    $prog = shift @files;
	}

	open my $fh, '>', $tmpfile or die "Cannot open >$tmpfile: $!";
	print $fh q{
        BEGIN {
            open STDERR, '>&', STDOUT
              or die "Can't dup STDOUT->STDERR: $!;";
        }
	};
	print $fh "\n#line 1\n";  # So the line numbers don't get messed up.
	print $fh $prog,"\n";
	close $fh or die "Cannot close $tmpfile: $!";
	my $results = runperl( stderr => 1, progfile => $tmpfile, $up
			       ? (switches => ["-I$up/lib", $switch], nolib => 1)
			       : (switches => [$switch])
			        );
	my $status = $?;
	$results =~ s/\n+$//;
	# allow expected output to be written as if $prog is on STDIN
	$results =~ s/$::tempfile_regexp/-/g;
	if ($^O eq 'VMS') {
	    # some tests will trigger VMS messages that won't be expected
	    $results =~ s/\n?%[A-Z]+-[SIWEF]-[A-Z]+,.*//;

	    # pipes double these sometimes
	    $results =~ s/\n\n/\n/g;
	}
	# bison says 'parse error' instead of 'syntax error',
	# various yaccs may or may not capitalize 'syntax'.
	$results =~ s/^(syntax|parse) error/syntax error/mig;
	# allow all tests to run when there are leaks
	$results =~ s/Scalars leaked: \d+\n//g;

	$expected =~ s/\n+$//;
	my $prefix = ($results =~ s#^PREFIX(\n|$)##) ;
	# any special options? (OPTIONS foo bar zap)
	my $option_regex = 0;
	my $option_random = 0;
	if ($expected =~ s/^OPTIONS? (.+)\n//) {
	    foreach my $option (split(' ', $1)) {
		if ($option eq 'regex') { # allow regular expressions
		    $option_regex = 1;
		}
		elsif ($option eq 'random') { # all lines match, but in any order
		    $option_random = 1;
		}
		else {
		    die "$0: Unknown OPTION '$option'\n";
		}
	    }
	}
	die "$0: can't have OPTION regex and random\n"
	    if $option_regex + $option_random > 1;
	my $ok = 0;
	if ($results =~ s/^SKIPPED\n//) {
	    print "$results\n" ;
	    $ok = 1;
	}
	elsif ($option_random) {
	    my @got = sort split "\n", $results;
	    my @expected = sort split "\n", $expected;

	    $ok = "@got" eq "@expected";
	}
	elsif ($option_regex) {
	    $ok = $results =~ /^$expected/;
	}
	elsif ($prefix) {
	    $ok = $results =~ /^\Q$expected/;
	}
	else {
	    $ok = $results eq $expected;
	}

	local $::TODO = $reason{todo};

	unless ($ok) {
	    my $err_line = "PROG: $switch\n$prog\n" .
			   "EXPECTED:\n$expected\n" .
			   "GOT:\n$results\n";
	    if ($::TODO) {
		$err_line =~ s/^/# /mg;
		print $err_line;  # Harness can't filter it out from STDERR.
	    }
	    else {
		print STDERR $err_line;
	    }
	}

	ok($ok);

	foreach (@temps) {
	    unlink $_ if $_;
	}
	foreach (@temp_path) {
	    File::Path::rmtree $_ if -d $_;
	}
    }
}

sub can_ok ($@) {
    my($proto, @methods) = @_;
    my $class = ref $proto || $proto;

    unless( @methods ) {
        return _ok( 0, _where(), "$class->can(...)" );
    }

    my @nok = ();
    foreach my $method (@methods) {
        local($!, $@);  # don't interfere with caller's $@
                        # eval sometimes resets $!
        eval { $proto->can($method) } || push @nok, $method;
    }

    my $name;
    $name = @methods == 1 ? "$class->can('$methods[0]')"
                          : "$class->can(...)";

    _ok( !@nok, _where(), $name );
}


# Call $class->new( @$args ); and run the result through isa_ok.
# See Test::More::new_ok
sub new_ok {
    my($class, $args, $obj_name) = @_;
    $args ||= [];
    $object_name = "The object" unless defined $obj_name;

    local $Level = $Level + 1;

    my $obj;
    my $ok = eval { $obj = $class->new(@$args); 1 };
    my $error = $@;

    if($ok) {
        isa_ok($obj, $class, $object_name);
    }
    else {
        ok( 0, "new() died" );
        diag("Error was:  $@");
    }

    return $obj;

}


sub isa_ok ($$;$) {
    my($object, $class, $obj_name) = @_;

    my $diag;
    $obj_name = 'The object' unless defined $obj_name;
    my $name = "$obj_name isa $class";
    if( !defined $object ) {
        $diag = "$obj_name isn't defined";
    }
    elsif( !ref $object ) {
        $diag = "$obj_name isn't a reference";
    }
    else {
        # We can't use UNIVERSAL::isa because we want to honor isa() overrides
        local($@, $!);  # eval sometimes resets $!
        my $rslt = eval { $object->isa($class) };
        if( $@ ) {
            if( $@ =~ /^Can't call method "isa" on unblessed reference/ ) {
                if( !UNIVERSAL::isa($object, $class) ) {
                    my $ref = ref $object;
                    $diag = "$obj_name isn't a '$class' it's a '$ref'";
                }
            } else {
                die <<WHOA;
WHOA! I tried to call ->isa on your object and got some weird error.
This should never happen.  Please contact the author immediately.
Here's the error.
$@
WHOA
            }
        }
        elsif( !$rslt ) {
            my $ref = ref $object;
            $diag = "$obj_name isn't a '$class' it's a '$ref'";
        }
    }

    _ok( !$diag, _where(), $name );
}

# Purposefully avoiding a closure.
sub __capture {
    push @::__capture, join "", @_;
}
    
sub capture_warnings {
    my $code = shift;

    local @::__capture;
    local $SIG {__WARN__} = \&__capture;
    &$code;
    return @::__capture;
}

# This will generate a variable number of tests.
# Use done_testing() instead of a fixed plan.
sub warnings_like {
    my ($code, $expect, $name) = @_;
    local $Level = $Level + 1;

    my @w = capture_warnings($code);

    cmp_ok(scalar @w, '==', scalar @$expect, $name);
    foreach my $e (@$expect) {
	if (ref $e) {
	    like(shift @w, $e, $name);
	} else {
	    is(shift @w, $e, $name);
	}
    }
    if (@w) {
	diag("Saw these additional warnings:");
	diag($_) foreach @w;
    }
}

sub _fail_excess_warnings {
    my($expect, $got, $name) = @_;
    local $Level = $Level + 1;
    # This will fail, and produce diagnostics
    is($expect, scalar @$got, $name);
    diag("Saw these warnings:");
    diag($_) foreach @$got;
}

sub warning_is {
    my ($code, $expect, $name) = @_;
    die sprintf "Expect must be a string or undef, not a %s reference", ref $expect
	if ref $expect;
    local $Level = $Level + 1;
    my @w = capture_warnings($code);
    if (@w > 1) {
	_fail_excess_warnings(0 + defined $expect, \@w, $name);
    } else {
	is($w[0], $expect, $name);
    }
}

sub warning_like {
    my ($code, $expect, $name) = @_;
    die sprintf "Expect must be a regexp object"
	unless ref $expect eq 'Regexp';
    local $Level = $Level + 1;
    my @w = capture_warnings($code);
    if (@w > 1) {
	_fail_excess_warnings(0 + defined $expect, \@w, $name);
    } else {
	like($w[0], $expect, $name);
    }
}

# Set a watchdog to timeout the entire test file
# NOTE:  If the test file uses 'threads', then call the watchdog() function
#        _AFTER_ the 'threads' module is loaded.
sub watchdog ($;$)
{
    my $timeout = shift;
    my $method  = shift || "";
    my $timeout_msg = 'Test process timed out - terminating';

    # Valgrind slows perl way down so give it more time before dying.
    $timeout *= 10 if $ENV{PERL_VALGRIND};

    my $pid_to_kill = $$;   # PID for this process

    if ($method eq "alarm") {
        goto WATCHDOG_VIA_ALARM;
    }

    # shut up use only once warning
    my $threads_on = $threads::threads && $threads::threads;

    # Don't use a watchdog process if 'threads' is loaded -
    #   use a watchdog thread instead
    if (!$threads_on) {

        # On Windows and VMS, try launching a watchdog process
        #   using system(1, ...) (see perlport.pod)
        if ($is_mswin || $is_vms) {
            # On Windows, try to get the 'real' PID
            if ($is_mswin) {
                eval { require Win32; };
                if (defined(&Win32::GetCurrentProcessId)) {
                    $pid_to_kill = Win32::GetCurrentProcessId();
                }
            }

            # If we still have a fake PID, we can't use this method at all
            return if ($pid_to_kill <= 0);

            # Launch watchdog process
            my $watchdog;
            eval {
                local $SIG{'__WARN__'} = sub {
                    _diag("Watchdog warning: $_[0]");
                };
                my $sig = $is_vms ? 'TERM' : 'KILL';
                my $cmd = _create_runperl( prog =>  "sleep($timeout);" .
                                                    "warn qq/# $timeout_msg" . '\n/;' .
                                                    "kill($sig, $pid_to_kill);");
                $watchdog = system(1, $cmd);
            };
            if ($@ || ($watchdog <= 0)) {
                _diag('Failed to start watchdog');
                _diag($@) if $@;
                undef($watchdog);
                return;
            }

            # Add END block to parent to terminate and
            #   clean up watchdog process
            eval "END { local \$! = 0; local \$? = 0;
                        wait() if kill('KILL', $watchdog); };";
            return;
        }

        # Try using fork() to generate a watchdog process
        my $watchdog;
        eval { $watchdog = fork() };
        if (defined($watchdog)) {
            if ($watchdog) {   # Parent process
                # Add END block to parent to terminate and
                #   clean up watchdog process
                eval "END { local \$! = 0; local \$? = 0;
                            wait() if kill('KILL', $watchdog); };";
                return;
            }

            ### Watchdog process code

            # Load POSIX if available
            eval { require POSIX; };

            # Execute the timeout
            sleep($timeout - 2) if ($timeout > 2);   # Workaround for perlbug #49073
            sleep(2);

            # Kill test process if still running
            if (kill(0, $pid_to_kill)) {
                _diag($timeout_msg);
                kill('KILL', $pid_to_kill);
            }

            # Don't execute END block (added at beginning of this file)
            $NO_ENDING = 1;

            # Terminate ourself (i.e., the watchdog)
            POSIX::_exit(1) if (defined(&POSIX::_exit));
            exit(1);
        }

        # fork() failed - fall through and try using a thread
    }

    # Use a watchdog thread because either 'threads' is loaded,
    #   or fork() failed
    if (eval {require threads; 1}) {
        'threads'->create(sub {
                # Load POSIX if available
                eval { require POSIX; };

                # Execute the timeout
                my $time_left = $timeout;
                do {
                    $time_left = $time_left - sleep($time_left);
                } while ($time_left > 0);

                # Kill the parent (and ourself)
                select(STDERR); $| = 1;
                _diag($timeout_msg);
                POSIX::_exit(1) if (defined(&POSIX::_exit));
                my $sig = $is_vms ? 'TERM' : 'KILL';
                kill($sig, $pid_to_kill);
            })->detach();
        return;
    }

    # If everything above fails, then just use an alarm timeout
WATCHDOG_VIA_ALARM:
    if (eval { alarm($timeout); 1; }) {
        # Load POSIX if available
        eval { require POSIX; };

        # Alarm handler will do the actual 'killing'
        $SIG{'ALRM'} = sub {
            select(STDERR); $| = 1;
            _diag($timeout_msg);
            POSIX::_exit(1) if (defined(&POSIX::_exit));
            my $sig = $is_vms ? 'TERM' : 'KILL';
            kill($sig, $pid_to_kill);
        };
    }
}

my $cp_0037 =   # EBCDIC code page 0037
    '\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x25\x0B\x0C\x0D\x0E\x0F' .
    '\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x3F\x27\x1C\x1D\x1E\x1F' .
    '\x40\x5A\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61' .
    '\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F' .
    '\x7C\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6' .
    '\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xBA\xE0\xBB\xB0\x6D' .
    '\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96' .
    '\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xC0\x4F\xD0\xA1\x07' .
    '\x20\x21\x22\x23\x24\x15\x06\x17\x28\x29\x2A\x2B\x2C\x09\x0A\x1B' .
    '\x30\x31\x1A\x33\x34\x35\x36\x08\x38\x39\x3A\x3B\x04\x14\x3E\xFF' .
    '\x41\xAA\x4A\xB1\x9F\xB2\x6A\xB5\xBD\xB4\x9A\x8A\x5F\xCA\xAF\xBC' .
    '\x90\x8F\xEA\xFA\xBE\xA0\xB6\xB3\x9D\xDA\x9B\x8B\xB7\xB8\xB9\xAB' .
    '\x64\x65\x62\x66\x63\x67\x9E\x68\x74\x71\x72\x73\x78\x75\x76\x77' .
    '\xAC\x69\xED\xEE\xEB\xEF\xEC\xBF\x80\xFD\xFE\xFB\xFC\xAD\xAE\x59' .
    '\x44\x45\x42\x46\x43\x47\x9C\x48\x54\x51\x52\x53\x58\x55\x56\x57' .
    '\x8C\x49\xCD\xCE\xCB\xCF\xCC\xE1\x70\xDD\xDE\xDB\xDC\x8D\x8E\xDF';

my $cp_1047 =   # EBCDIC code page 1047
    '\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x15\x0B\x0C\x0D\x0E\x0F' .
    '\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x3F\x27\x1C\x1D\x1E\x1F' .
    '\x40\x5A\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61' .
    '\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F' .
    '\x7C\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6' .
    '\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xAD\xE0\xBD\x5F\x6D' .
    '\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96' .
    '\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xC0\x4F\xD0\xA1\x07' .
    '\x20\x21\x22\x23\x24\x25\x06\x17\x28\x29\x2A\x2B\x2C\x09\x0A\x1B' .
    '\x30\x31\x1A\x33\x34\x35\x36\x08\x38\x39\x3A\x3B\x04\x14\x3E\xFF' .
    '\x41\xAA\x4A\xB1\x9F\xB2\x6A\xB5\xBB\xB4\x9A\x8A\xB0\xCA\xAF\xBC' .
    '\x90\x8F\xEA\xFA\xBE\xA0\xB6\xB3\x9D\xDA\x9B\x8B\xB7\xB8\xB9\xAB' .
    '\x64\x65\x62\x66\x63\x67\x9E\x68\x74\x71\x72\x73\x78\x75\x76\x77' .
    '\xAC\x69\xED\xEE\xEB\xEF\xEC\xBF\x80\xFD\xFE\xFB\xFC\xBA\xAE\x59' .
    '\x44\x45\x42\x46\x43\x47\x9C\x48\x54\x51\x52\x53\x58\x55\x56\x57' .
    '\x8C\x49\xCD\xCE\xCB\xCF\xCC\xE1\x70\xDD\xDE\xDB\xDC\x8D\x8E\xDF';

my $cp_bc = # EBCDIC code page POSiX-BC
    '\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x15\x0B\x0C\x0D\x0E\x0F' .
    '\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x3F\x27\x1C\x1D\x1E\x1F' .
    '\x40\x5A\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61' .
    '\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F' .
    '\x7C\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6' .
    '\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xBB\xBC\xBD\x6A\x6D' .
    '\x4A\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96' .
    '\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xFB\x4F\xFD\xFF\x07' .
    '\x20\x21\x22\x23\x24\x25\x06\x17\x28\x29\x2A\x2B\x2C\x09\x0A\x1B' .
    '\x30\x31\x1A\x33\x34\x35\x36\x08\x38\x39\x3A\x3B\x04\x14\x3E\x5F' .
    '\x41\xAA\xB0\xB1\x9F\xB2\xD0\xB5\x79\xB4\x9A\x8A\xBA\xCA\xAF\xA1' .
    '\x90\x8F\xEA\xFA\xBE\xA0\xB6\xB3\x9D\xDA\x9B\x8B\xB7\xB8\xB9\xAB' .
    '\x64\x65\x62\x66\x63\x67\x9E\x68\x74\x71\x72\x73\x78\x75\x76\x77' .
    '\xAC\x69\xED\xEE\xEB\xEF\xEC\xBF\x80\xE0\xFE\xDD\xFC\xAD\xAE\x59' .
    '\x44\x45\x42\x46\x43\x47\x9C\x48\x54\x51\x52\x53\x58\x55\x56\x57' .
    '\x8C\x49\xCD\xCE\xCB\xCF\xCC\xE1\x70\xC0\xDE\xDB\xDC\x8D\x8E\xDF';

my $straight =  # Avoid ranges
    '\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F' .
    '\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F' .
    '\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F' .
    '\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F' .
    '\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F' .
    '\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F' .
    '\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F' .
    '\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F' .
    '\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F' .
    '\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F' .
    '\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF' .
    '\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF' .
    '\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF' .
    '\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF' .
    '\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF' .
    '\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF';

# The following 2 functions allow tests to work on both EBCDIC and
# ASCII-ish platforms.  They convert string scalars between the native
# character set and the set of 256 characters which is usually called
# Latin1.
#
# These routines don't work on UTF-EBCDIC and UTF-8.

sub native_to_latin1($) {
    my $string = shift;

    return $string if ord('^') == 94;   # ASCII, Latin1
    my $cp;
    if (ord('^') == 95) {    # EBCDIC 1047
        $cp = \$cp_1047;
    }
    elsif (ord('^') == 106) {   # EBCDIC POSIX-BC
        $cp = \$cp_bc;
    }
    elsif (ord('^') == 176)  {   # EBCDIC 037 */
        $cp = \$cp_0037;
    }
    else {
        die "Unknown native character set";
    }

    eval '$string =~ tr/' . $$cp . '/' . $straight . '/';
    return $string;
}

sub latin1_to_native($) {
    my $string = shift;

    return $string if ord('^') == 94;   # ASCII, Latin1
    my $cp;
    if (ord('^') == 95) {    # EBCDIC 1047
        $cp = \$cp_1047;
    }
    elsif (ord('^') == 106) {   # EBCDIC POSIX-BC
        $cp = \$cp_bc;
    }
    elsif (ord('^') == 176)  {   # EBCDIC 037 */
        $cp = \$cp_0037;
    }
    else {
        die "Unknown native character set";
    }

    eval '$string =~ tr/' . $straight . '/' . $$cp . '/';
    return $string;
}

sub ord_latin1_to_native {
    # given an input code point, return the platform's native
    # equivalent value.  Anything above latin1 is itself.

    my $ord = shift;
    return $ord if $ord > 255;
    return ord latin1_to_native(chr $ord);
}

sub ord_native_to_latin1 {
    # given an input platform code point, return the latin1 equivalent value.
    # Anything above latin1 is itself.

    my $ord = shift;
    return $ord if $ord > 255;
    return ord native_to_latin1(chr $ord);
}

1;
