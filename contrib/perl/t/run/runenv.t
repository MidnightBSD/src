#!./perl
#
# Tests for Perl run-time environment variable settings
#
# $PERL5OPT, $PERL5LIB, etc.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    unless ($Config{'d_fork'}) {
        print "1..0 # Skip: no fork\n";
	    exit 0;
    }
    require './test.pl'
}

plan tests => 17;

my $STDOUT = tempfile();
my $STDERR = tempfile();
my $PERL = $ENV{PERL} || './perl';
my $FAILURE_CODE = 119;

delete $ENV{PERLLIB};
delete $ENV{PERL5LIB};
delete $ENV{PERL5OPT};


sub runperl_and_capture {
  local *F;
  my ($env, $args) = @_;
  unshift @$args, '-I../lib';

  local %ENV = %ENV;
  delete $ENV{PERLLIB};
  delete $ENV{PERL5LIB};
  delete $ENV{PERL5OPT};
  my $pid = fork;
  return (0, "Couldn't fork: $!") unless defined $pid;   # failure
  if ($pid) {                   # parent
    my ($actual_stdout, $actual_stderr);
    wait;
    return (0, "Failure in child.\n") if ($?>>8) == $FAILURE_CODE;

    open F, "< $STDOUT" or return (0, "Couldn't read $STDOUT file");
    { local $/; $actual_stdout = <F> }
    open F, "< $STDERR" or return (0, "Couldn't read $STDERR file");
    { local $/; $actual_stderr = <F> }

    return ($actual_stdout, $actual_stderr);
  } else {                      # child
    for my $k (keys %$env) {
      $ENV{$k} = $env->{$k};
    }
    open STDOUT, "> $STDOUT" or exit $FAILURE_CODE;
    open STDERR, "> $STDERR" or it_didnt_work();
    { exec $PERL, @$args }
    it_didnt_work();
  }
}

# Run perl with specified environment and arguments returns a list.
# First element is true if Perl's stdout and stderr match the
# supplied $stdout and $stderr argument strings exactly.
# second element is an explanation of the failure
sub runperl {
  local *F;
  my ($env, $args, $stdout, $stderr) = @_;
  my ($actual_stdout, $actual_stderr) = runperl_and_capture($env, $args);
  if ($actual_stdout ne $stdout) {
    return (0, "Stdout mismatch: expected [$stdout], saw [$actual_stdout]");
  } elsif ($actual_stderr ne $stderr) {
    return (0, "Stderr mismatch: expected [$stderr], saw [$actual_stderr]");
  } else {
    return 1;                 # success
  }
}

sub it_didnt_work {
    print STDOUT "IWHCWJIHCI\cNHJWCJQWKJQJWCQW\n";
    exit $FAILURE_CODE;
}

sub try {
  my ($success, $reason) = runperl(@_);
  $reason =~ s/\n/\\n/g if defined $reason;
  local $::Level = $::Level + 1;
  ok( $success, $reason );
}

#  PERL5OPT    Command-line options (switches).  Switches in
#                    this variable are taken as if they were on
#                    every Perl command line.  Only the -[DIMUdmtw]
#                    switches are allowed.  When running taint
#                    checks (because the program was running setuid
#                    or setgid, or the -T switch was used), this
#                    variable is ignored.  If PERL5OPT begins with
#                    -T, tainting will be enabled, and any
#                    subsequent options ignored.

try({PERL5OPT => '-w'}, ['-e', 'print $::x'],
    "", 
    qq{Name "main::x" used only once: possible typo at -e line 1.\nUse of uninitialized value \$x in print at -e line 1.\n});

try({PERL5OPT => '-Mstrict'}, ['-e', 'print $::x'],
    "", "");

try({PERL5OPT => '-Mstrict'}, ['-e', 'print $x'],
    "", 
    qq{Global symbol "\$x" requires explicit package name at -e line 1.\nExecution of -e aborted due to compilation errors.\n});

# Fails in 5.6.0
try({PERL5OPT => '-Mstrict -w'}, ['-e', 'print $x'],
    "", 
    qq{Global symbol "\$x" requires explicit package name at -e line 1.\nExecution of -e aborted due to compilation errors.\n});

# Fails in 5.6.0
try({PERL5OPT => '-w -Mstrict'}, ['-e', 'print $::x'],
    "", 
    <<ERROR
Name "main::x" used only once: possible typo at -e line 1.
Use of uninitialized value \$x in print at -e line 1.
ERROR
    );

# Fails in 5.6.0
try({PERL5OPT => '-w -Mstrict'}, ['-e', 'print $::x'],
    "", 
    <<ERROR
Name "main::x" used only once: possible typo at -e line 1.
Use of uninitialized value \$x in print at -e line 1.
ERROR
    );

try({PERL5OPT => '-MExporter'}, ['-e0'],
    "", 
    "");

# Fails in 5.6.0
try({PERL5OPT => '-MExporter -MExporter'}, ['-e0'],
    "", 
    "");

try({PERL5OPT => '-Mstrict -Mwarnings'}, 
    ['-e', 'print "ok" if $INC{"strict.pm"} and $INC{"warnings.pm"}'],
    "ok",
    "");

try({PERL5OPT => '-w -w'},
    ['-e', 'print $ENV{PERL5OPT}'],
    '-w -w',
    '');

try({PERL5OPT => '-t'},
    ['-e', 'print ${^TAINT}'],
    '-1',
    '');

try({PERLLIB => "foobar$Config{path_sep}42"},
    ['-e', 'print grep { $_ eq "foobar" } @INC'],
    'foobar',
    '');

try({PERLLIB => "foobar$Config{path_sep}42"},
    ['-e', 'print grep { $_ eq "42" } @INC'],
    '42',
    '');

try({PERL5LIB => "foobar$Config{path_sep}42"},
    ['-e', 'print grep { $_ eq "foobar" } @INC'],
    'foobar',
    '');

try({PERL5LIB => "foobar$Config{path_sep}42"},
    ['-e', 'print grep { $_ eq "42" } @INC'],
    '42',
    '');

try({PERL5LIB => "foo",
     PERLLIB => "bar"},
    ['-e', 'print grep { $_ eq "foo" } @INC'],
    'foo',
    '');

try({PERL5LIB => "foo",
     PERLLIB => "bar"},
    ['-e', 'print grep { $_ eq "bar" } @INC'],
    '',
    '');

# PERL5LIB tests with included arch directories still missing

END {
    1 while unlink $STDOUT;
    1 while unlink $STDERR;
}
