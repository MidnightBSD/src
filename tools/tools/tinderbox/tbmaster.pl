#!/usr/bin/perl -Tw
#-
# Copyright (c) 2003-2014 Dag-Erling Smørgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD: user/des/tinderbox/tbmaster.pl 266153 2014-05-15 16:17:21Z des $
# $MidnightBSD$

use v5.10.1;
use strict;
use Fcntl qw(:DEFAULT :flock);
use POSIX;
use Getopt::Long;
use Storable qw(dclone);

my $VERSION	= "2.22";
my $COPYRIGHT	= "Copyright (c) 2003-2014 Dag-Erling Smørgrav. " .
		  "All rights reserved.";

my $BACKLOG	= 20;

my $abbreviate;			# Abbreviate path names in log file
my @configs;			# Names of requested configations
my $dump;			# Dump configuration and exit
my $etcdir;			# Configuration directory
my $lockfile;			# Lock file name
my $lock;			# Lock file descriptor
my $hostname;			# Hostname
my $ncpu;			# Number of CPUs
my %platforms;			# Specific platforms to build

my %INITIAL_CONFIG = (
    'BRANCHES'	=> [ 'HEAD' ],
    'CFLAGS'	=> '',
    'COPTFLAGS'	=> '',
    'COMMENT'	=> '',
    'ENV'	=> [ ],
    'HOSTNAME'	=> '',
    'JOBS'	=> '',
    'LOGDIR'	=> '${SANDBOX}/logs',
    'NCPU'	=> '',
    'OBJDIR'	=> '',
    'OPTIONS'	=> [ ],
    'PATCH'	=> '',
    'PLATFORMS'	=> [ 'i386' ],
    'RECIPIENT'	=> [ '${SENDER}' ],
    'SANDBOX'	=> '/tmp/tinderbox',
    'SENDER'	=> '',
    'SRCDIR'	=> '',
    'SUBJECT'	=> 'Tinderbox failure on ${arch}/${machine}',
    'SVNBASE'	=> '',
    'TARGETS'	=> [ 'update', 'world' ],
    'TIMEOUT'   => '',
    'TINDERBOX'	=> '${HOME}/bin/tinderbox',
    'URLBASE'	=> '',
);
my %NUMERIC_OPTIONS = map { $_ => 1 } qw(JOBS NCPU TIMEOUT);
my %PATHNAME_OPTIONS =
    map { $_ => 1 } qw(LOGDIR OBJDIR PATCH SANDBOX SRCDIR TINDERBOX);
my %WORD_OPTIONS = map { $_ => 1 } qw(PLATFORMS TARGETS);
my %CONFIG;

#
# Expand a path
#
sub realpath($;$);
sub realpath($;$) {
    my $path = shift;
    my $base = shift || "";

    my $realpath = ($path =~ m|^/|) ? "" : $base;
    foreach my $part (split('/', $path)) {
	if ($part eq '' || $part eq '.') {
	    # nothing
	} elsif ($part eq '..') {
	    $realpath =~ s|/[^/]+$||
		or die("'$path' is not a valid path relative to '$base'\n");
	} elsif (-l "$realpath/$part") {
	    my $target = readlink("$realpath/$part")
		or die("unable to resolve symlink '$realpath/$part': $!\n");
	    $realpath = realpath($target, $realpath);
	} else {
	    $part =~ m/^([\w.-]+)$/
		or die("unsafe path '$realpath/$part'\n");
	    $realpath .= "/$1";
	}
    }
    return $realpath;
}

#
# Perform variable expansion
#
sub expand($);
sub expand($) {
    my $key = shift;

    return "??$key??"
	unless exists($CONFIG{uc($key)});
    my $value = $CONFIG{uc($key)};
    my @elements = ref($value) ? @{$value} : $value;
    my @expanded;
    while (@elements) {
	my $elem = shift(@elements);
	if (ref($elem)) {
	    # prepend to queue for further processing
	    unshift(@elements, @{$elem});
	} elsif ($elem =~ m/^\%\%(\w+)\%\%$/ || $elem =~ m/^\%\{(\w+)\}$/) {
	    # prepend to queue for further processing
	    # note - can expand to a list
	    unshift(@elements, expand($1));
	} else {
	    $elem =~ s/\$ENV\{(\w+)\}/$ENV{$1}/g;
	    $elem =~ s/\%\%(\w+)\%\%/expand($1)/eg;
	    $elem =~ s/\$\{(\w+)\}/expand($1)/eg;
	    push(@expanded, $elem);
	}
    }

    # Upper / lower case
    if ($key !~ m/[A-Z]/) {
	@expanded = map { lc($_) } @expanded;
    }

    # Validate and untaint expanded value(s)
    if ($NUMERIC_OPTIONS{uc($key)}) {
	@expanded = map {
	    m/^(\d+|)$/
		or die("invalid value for numeric variable $key: $_\n");
	    $1
	} @expanded;
    } elsif ($PATHNAME_OPTIONS{uc($key)}) {
	@expanded = map {
	    m@^((?:/+[\w.-]+)+/*|)$@
		or die("invalid value for pathname variable $key: $_\n");
	    $1
	} @expanded;
    } elsif ($WORD_OPTIONS{uc($key)}) {
	@expanded = map {
	    # hack - support not only "word" but also "word/word" so
	    # platform designations will pass the test.
	    m@^([\w.-]+(?:/[\w.-]+)?|)$@
		or die("invalid value for word variable $key: $_\n");
	    $1
	} @expanded;
    }

    # Verify single / multiple and return result
    if (ref($value)) {
	return @expanded;
    } elsif (@expanded != 1) {
	die("expand($key): expected one value, got ", int(@expanded), "\n");
    } else {
	return $expanded[0];
    }
}

#
# Reset the configuration to initial values
#
sub clearconf() {

    %CONFIG = %{dclone(\%INITIAL_CONFIG)};
}

#
# Read in a configuration file
#
sub readconf($);
sub readconf($) {
    my $fn = shift;

    open(my $fh, '<', $fn)
	or return undef;
    my $line = "";
    my $n = 0;
    while (<$fh>) {
	++$n;
	chomp();
	s/\s*(\#.*)?$//;
	$line .= $_;
	if (length($line) && $line !~ s/\\$/ /) {
	    if ($line =~ m/^include\s+([\w-]+)$/) {
		readconf("$1.rc")
		    or die("$fn: include $1: $!\n");
	    } elsif ($line =~ m/^(\w+)\s*([+-]?=)\s*(.*)$/) {
		my ($key, $op, $val) = (uc($1), $2, $3);
		$val = ''
		    unless defined($val);
		die("$fn: $key is not a known keyword on line $n\n")
		    unless (exists($CONFIG{$key}));
		if (ref($CONFIG{$key})) {
		    my @a = split(/\s*,\s*/, $val);
		    foreach (@a) {
			s/^\'([^\']*)\'$/$1/;
		    }
		    if ($op eq '=') {
			$CONFIG{$key} = \@a;
		    } elsif ($op eq '+=') {
			push(@{$CONFIG{$key}}, @a);
		    } elsif ($op eq '-=') {
			my %a = map { $_ => $_ } @a;
			@{$CONFIG{$key}} =
			    grep { !exists($a{$_}) } @{$CONFIG{$key}};
		    } else {
			die("can't happen\n");
		    }
		} else {
		    $val =~ s/^\'([^\']*)\'$/$1/;
		    if ($op eq '=') {
			$CONFIG{$key} = $val;
		    } elsif ($op eq '+=' || $op eq '-=') {
			die("$fn: $key is not an array on line $n\n");
		    } else {
			die("can't happen\n");
		    }
		}
	    } else {
		die("$fn: syntax error on line $n\n")
	    }
	    $line = "";
	}
    }
    close($fh);
    return 1;
}

#
# Record a tinderbox result in the history file
#
sub history($$$) {
    my $start = shift;
    my $end = shift;
    my $success = shift;

    my $history = expand('HOSTNAME') . "\t";
    $history .= expand('CONFIG') . "\t";
    $history .= strftime("%Y-%m-%d %H:%M:%S\t", localtime($start));
    $history .= strftime("%Y-%m-%d %H:%M:%S\t", localtime($end));
    $history .= expand('ARCH') . "\t";
    $history .= expand('MACHINE') . "\t";
    $history .= expand('BRANCH') . "\t";
    $history .= $success ? "OK\n" : "FAIL\n";

    my $fn = expand('LOGDIR') . "/history";
    if (open(my $fh, '>>', $fn)) {
	print($fh $history);
	close($fh);
    } else {
	print(STDERR "failed to record result to history file:\n$history\n");
    }
}

#
# Report a tinderbox failure
#
sub report($$$$) {
    my $sender = shift;
    my $recipient = shift;
    my $subject = shift;
    my $message = shift;

    if (!$message) {
	print(STDERR "[empty report, not sent by email]\n\n]");
	return;
    }
    if (length($message) < 128) {
	print(STDERR "[suspiciously short report, not sent by email]\n\n");
	print(STDERR $message);
	return;
    }

    if (open(my $pipe, '|-', qw(/usr/sbin/sendmail -i -t -f), $sender)) {
	print($pipe "Sender: $sender\n");
	print($pipe "From: $sender\n");
	print($pipe "To: $recipient\n");
	print($pipe "Subject: $subject\n");
	print($pipe "Precedence: bulk\n");
	print($pipe "\n");
	print($pipe "$message\n");
	close($pipe);
    } else {
	print(STDERR "[failed to send report by email]\n\n");
	print(STDERR $message);
    }
}

#
# Run the tinderbox
#
sub tinderbox($$$) {
    my $branch = shift;
    my $arch = shift;
    my $machine = shift;

    my $config = expand('CONFIG');
    my $start = time();

    $0 = "tbmaster [$config]: building $branch for $arch/$machine";

    $CONFIG{'BRANCH'} = $branch;
    $CONFIG{'ARCH'} = $arch;
    $CONFIG{'MACHINE'} = $machine;

    # Open log files: one for the full log and one for the summary
    my $logdir = expand('LOGDIR');
    if (!-d $logdir) {
	die("nonexistent log directory: $logdir\n");
    }
    my $logname = "tinderbox-$config-$branch-$arch-$machine";
    my $logbase = "$logdir/$logname";
    my $full;
    if (!open($full, '>', "$logbase.full.$$")) {
	warn("$logbase.full.$$: $!\n");
	return undef;
    }
    select($full);
    $| = 1;
    select(STDOUT);
    my $brief;
    if (!open($brief, '>', "$logbase.brief.$$")) {
	warn("$logbase.brief.$$: $!\n");
	return undef;
    }
    select($brief);
    $| = 1;
    select(STDOUT);

    # Open a pipe for the tinderbox process
    my ($rpipe, $wpipe);
    if (!pipe($rpipe, $wpipe)) {
	warn("pipe(): $!\n");
	unlink("$logbase.brief.$$");
	close($brief);
	unlink("$logbase.full.$$");
	close($full);
	return undef;
    }

    # Fork and start the tinderbox
    my @args = expand('OPTIONS');
    push(@args, "--hostname=" . expand('HOSTNAME'));
    push(@args, "--sandbox=" . realpath(expand('SANDBOX')));
    push(@args, "--srcdir=" . realpath(expand('SRCDIR')))
	if ($CONFIG{'SRCDIR'});
    push(@args, "--objdir=" . realpath(expand('OBJDIR')))
	if ($CONFIG{'OBJDIR'});
    push(@args, "--arch=$arch");
    push(@args, "--machine=$machine");
    push(@args, "--repository=" . expand('REPOSITORY'))
	if ($CONFIG{'REPOSITORY'});
    push(@args, "--branch=$branch");
    push(@args, "--patch=" . expand('PATCH'))
	if ($CONFIG{'PATCH'});
    push(@args, "--jobs=" . expand('JOBS'))
	if ($CONFIG{'JOBS'});
    push(@args, "--svnbase=" . expand('SVNBASE'))
	if ($CONFIG{'SVNBASE'});
    push(@args, "--timeout=" . expand('TIMEOUT'))
	if ($CONFIG{'TIMEOUT'});
    push(@args, expand('TARGETS'));
    push(@args, expand('ENV'));
    push(@args, "CFLAGS=" . expand('CFLAGS'))
	if ($CONFIG{'CFLAGS'});
    push(@args, "COPTFLAGS=" . expand('COPTFLAGS'))
	if ($CONFIG{'COPTFLAGS'});
    my $pid = fork();
    if (!defined($pid)) {
	warn("fork(): $!\n");
	unlink("$logbase.brief.$$");
	close($brief);
	unlink("$logbase.full.$$");
	close($full);
	return undef;
    } elsif ($pid == 0) {
	close($rpipe);
	open(STDOUT, '>&', $wpipe);
	open(STDERR, '>&', $wpipe);
	$| = 1;
	exec(expand('TINDERBOX'), @args);
	die("exec(): $!\n");
    }

    # Process the output
    close($wpipe);
    my @lines = ();
    my $error = 0;
    my $summary = "";
    my $root = realpath(expand('SANDBOX') . "/$branch/$arch/$machine");
    my $srcdir = realpath(expand('SRCDIR') || "$root/src");
    my $objdir = realpath(expand('OBJDIR') || "$root/obj");
    while (<$rpipe>) {
	if ($abbreviate) {
	    s/\Q$srcdir\E/\/src/go;
	    s/\Q$objdir\E/\/obj/go;
	}
	print($full $_);
	if (/^TB ---/ || /^>>> /) {
	    if ($error) {
		$summary .= join('', @lines);
		print($brief join('', @lines));
		@lines = ();
		$error = 0;
	    }
	    $summary .= $_;
	    print($brief $_);
	    @lines = ();
	    next;
	}
	if (/^\*\*\*( \[.*?\])? (Error code|Stopped|Signal)/ &&
	    !/\(ignored\)/) {
	    $error = 1;
	}
	if (@lines > $BACKLOG && !$error) {
	    shift(@lines);
	    $lines[0] = "[...]\n";
	}
	push(@lines, $_);
    }
    close($rpipe);
    if ($error) {
	$summary .= join('', @lines);
	print($brief join('', @lines));
    }

    # Done...
    if (waitpid($pid, 0) == -1) {
	warn("waitpid(): $!\n");
    } elsif ($? & 0xff) {
	my $msg = "tinderbox caught signal " . ($? & 0x7f) . "\n";
	print($brief $msg);
	print($full $msg);
	$error = 1;
    } elsif ($? >> 8) {
	my $msg = "tinderbox returned exit code " . ($? >> 8) . "\n";
	print($brief $msg);
	print($full $msg);
	$error = 1;
    }
    close($brief);
    close($full);

    my $end = time();

    # Record result in history file
    history($start, $end, !$error);

    # Filter recipients
    my @recipients = expand('RECIPIENT');
    if (!$ENV{'MAGIC_SAUCE'} ||
	$ENV{'MAGIC_SAUCE'} ne 'MIDNIGHTBSD_TINDERBOX') {
	@recipients = grep { ! m/\@midnightbsd.org\.org/i } @recipients;
    }

    # Mail out error reports
    if ($error && @recipients) {
	my $sender = expand('SENDER');
	my $recipient = join(', ', @recipients);
	my $subject = expand('SUBJECT');
	if ($CONFIG{'URLBASE'}) {
	    $summary .= "\n\n" . expand('URLBASE') . "$logname.full";
	}
	report($sender, $recipient, $subject, $summary);
    }

    rename("$logbase.full.$$", "$logbase.full");
    rename("$logbase.brief.$$", "$logbase.brief");
}

#
# Open and lock a file reliably
#
sub open_locked($;$$) {
    my $fn = shift;		# File name
    my $mode = shift;		# Open mode
    my $perm = shift;		# File permissions

    my $fh;			# File handle
    my (@sb1, @sb2);		# File status

    for (;; close($fh)) {
	open($fh, $mode, $fn)
	    or last;
	if (!(@sb1 = stat($fh))) {
	    # Huh? shouldn't happen
	    last;
	}
	if (!flock($fh, LOCK_EX|LOCK_NB)) {
	    # A failure here means the file can't be locked, or
	    # something really weird happened, so just give up.
	    last;
	}
	if (!(@sb2 = stat($fn))) {
	    # File was pulled from under our feet, though it may
	    # reappear in the next pass
	    next;
	}
	if ($sb1[0] != $sb2[0] || $sb1[1] != $sb2[1]) {
	    # File changed under our feet, try again
	    next;
	}
	chmod($fh, $perm)
	    if defined($perm);
	return $fh;
    }
    close($fh);
    return undef;
}

#
# Print a usage message and exit
#
sub usage() {

    (my $self = $0) =~ s|^.*/||;
    print(STDERR "This is the MidnightBSD tinderbox manager, version $VERSION.
$COPYRIGHT

Usage:
  $self [options] [parameters]

Options:
  -d, --dump                    Dump the processed configuration

Parameters:
  -a, --abbreviate		Abbreviate path names in log file
  -c, --config=NAME             Configuration name
  -e, --etcdir=DIR              Configuration directory
  -l, --lockfile=FILE           Lock file name
  -n, --ncpu=NUM                Number of CPUs available

");
    exit(1);
}

#
# Main loop
#
sub tbmaster($) {
    my $config = shift;

    clearconf();
    readconf('default.rc');
    readconf("$config.rc")
	or die("$config.rc: $!\n");
    readconf('site.rc');
    $CONFIG{'CONFIG'} = $config;
    $CONFIG{'ETCDIR'} = $etcdir;

    if ($dump) {
	foreach my $key (sort(keys(%CONFIG))) {
	    printf("%-12s = ", uc($key));
	    if (ref($CONFIG{$key})) {
		print(join(", ", @{$CONFIG{$key}}));
	    } else {
		print($CONFIG{$key});
	    }
	    print("\n");
	}
	return;
    }

    if (!length(expand('TINDERBOX')) || !-x expand('TINDERBOX')) {
	die("Where is the tinderbox script?\n");
    }

    # Check stop file
    my $stopfile = expand('SANDBOX') . "/stop";
    my @jobs;
    foreach my $branch (expand('BRANCHES')) {
	foreach my $platform (expand('PLATFORMS')) {
	    next if (%platforms && !$platforms{$platform});
	    my ($arch, $machine) = split('/', $platform, 2);
	    $machine = $arch
		unless defined($machine);
	    push(@jobs, [ $branch, $arch, $machine ]);
	}
    }

    # Main loop: start as many concurrent jobs as permitted, then keep
    # starting new jobs as soon as existing jobs terminate, until all
    # jobs have terminated and there are none left in the queue.
    $0 = "tbmaster [$config]: supervisor";
    my %children;
    my $done = 0;
    while (@jobs || keys(%children)) {
	# start more children if we can
	while (@jobs && keys(%children) < expand('NCPU')) {
	    my ($branch, $arch, $machine) = @{shift(@jobs)};
	    if (-e $stopfile || -e "$stopfile.$branch" ||
		-e "$stopfile.$arch" || -e "$stopfile.$arch.$machine") {
		warn("stop file found, skipping $branch $arch/$machine\n");
		next;
	    }
	    my $child = fork();
	    if (!defined($child)) {
		die("fork(): $!\n");
	    } elsif ($child == 0) {
		tinderbox($branch, $arch, $machine);
		exit(0);
	    } else {
		$children{$child} = [ $branch, $arch, $machine ];
	    }
	    warn("forked child $child for $branch $arch/$machine\n");
	}
	$0 = "tbmaster [$config]: supervisor (" .
	    keys(%children) . " running, " .
	    @jobs . " pending, " .
	    $done . " completed)";
	# wait for a child to terminate
	if (keys(%children)) {
	    my $child = wait();
	    if ($child > 0) {
		my ($branch, $arch, $machine) = @{$children{$child}};
		warn("child $child for $branch $arch/$machine terminated\n");
		delete($children{$child});
		++$done;
	    }
	}
    }
}

#
# Read the input from a command
#
sub slurp(@) {
    my @cmdline = @_;

    if (open(my $pipe, '-|', @cmdline)) {
	local $/;
	my $input = <$pipe>;
	close($pipe);
	return $input;
    }
    return undef;
}

#
# Main
#
MAIN:{
    # Set defaults
    $ENV{'TZ'} = "UTC";
    $ENV{'PATH'} = "/usr/bin:/usr/sbin:/bin:/sbin";
    if ($ENV{'HOME'} =~ m/^((?:\/[\w\.-]+)+)\/*$/) {
	$INITIAL_CONFIG{'HOME'} = realpath($1);
	$etcdir = "$1/etc";
	$ENV{'PATH'} = "$1/bin:$ENV{'PATH'}"
	    if (-d "$1/bin");
    }

    # Get options
    {Getopt::Long::Configure("auto_abbrev", "bundling");}
    GetOptions(
	"a|abbreviate!"		=> \$abbreviate,
	"c|config=s"		=> \@configs,
	"d|dump"		=> \$dump,
	"e|etcdir=s"		=> \$etcdir,
	"h|hostname=s"		=> \$hostname,
	"l|lockfile=s"		=> \$lockfile,
	"n|ncpu=i"		=> \$ncpu,
	) or usage();

    # Subsequent arguments are platforms to build
    foreach (@ARGV) {
	if (m/^(\w+(?:\/\w+)?)$/) {
	    $platforms{$1} = 1;
	} else {
	    die("invalid platform: $_\n");
	}
    }

    # Get / check hostname
    if (!$hostname) {
	$hostname = slurp(qw(/usr/bin/uname -n));
    }
    if ($hostname &&
	$hostname =~ m/^\s*([a-z][0-9a-z-]+(?:\.[a-z][0-9a-z-]+)*)\s*$/s) {
	$hostname = $1;
    } else {
	$hostname = 'unknown';
    }
    $INITIAL_CONFIG{'HOSTNAME'} = $hostname;

    # Get / check number of CPUs
    if (!$ncpu) {
	$ncpu = slurp(qw(/sbin/sysctl -n hw.ncpu));
    }
    if ($ncpu && $ncpu =~ m/^\s*(\d+)\s*$/s) {
	$ncpu = int($1);
    } else {
	$ncpu = 1;
    }
    $INITIAL_CONFIG{'NCPU'} = $ncpu;

    # Check options
    if (@configs) {
	@configs = split(/,/, join(',', @configs));
    } else {
	$configs[0] = $hostname;
	chomp($configs[0]);
	$configs[0] =~ s/^(\w+)(\..*)?/$1/;
    }
    if (defined($etcdir)) {
	if ($etcdir !~ m/^([\w\/\.-]+)$/) {
	    die("invalid etcdir\n");
	}
	$etcdir = $1;
	chdir($etcdir)
	    or die("$etcdir: $!\n");
    }
    for (my $n = 0; $n < @configs; ++$n) {
	$configs[$n] =~ m/^([\w-]+)$/
	    or die("invalid config: $configs[$n]\n");
	$configs[$n] = $1;
    }

    # Acquire lock
    if (defined($lockfile)) {
	if ($lockfile !~ m/^([\w\/\.-]+)$/) {
	    die("invalid lockfile\n");
	}
	$lockfile = $1;
	$lock = open_locked($lockfile, '>', 0600)
	    or die("unable to acquire lock on $lockfile\n");
	# Lock will be released upon termination.
    }

    # Run all specified or implied configurations
    foreach my $config (@configs) {
	tbmaster($config);
    }
    exit(0);
}
