#!/usr/bin/perl -Tw
#-
# Copyright (c) 2003-2012 Dag-Erling Smørgrav
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
# $FreeBSD: src/tools/tools/tinderbox/tinderbox.pl,v 1.42 2005/04/30 18:22:12 des Exp $
# $MidnightBSD: src/tools/tools/tinderbox/tinderbox.pl,v 1.3 2008/03/07 04:20:26 laffer1 Exp $
#

use v5.10.1;
use strict;
use Fcntl qw(:DEFAULT :flock);
use POSIX;
use Getopt::Long;

my $VERSION	= "2.10";
my $COPYRIGHT	= "Copyright (c) 2003-2012 Dag-Erling Smørgrav. " .
		  "All rights reserved.";

my $arch;			# Target architecture
my $branch;			# CVS branch to check out
my $cvsup;			# Name of CVSup server
my $destdir;			# Destination directory
my $jobs;			# Number of paralell jobs
my $hostname;			# Name of the host running the tinderbox
my $logfile;			# Path to log file
my $machine;			# Target machine
my $objdir;			# Location of object tree
my $patch;			# Patch to apply before building
my $repository;			# Location of CVS repository
my $sandbox;			# Location of sandbox
my $srcdir;			# Location of source tree
my $svnbase;			# Subversion base URL
my $timeout;			# Timeout in seconds
my $verbose;			# Verbose mode

my %children;

my %userenv;

my %cmds = (
    'clean'	=> 0, 'preclean'	=> 0, 'postclean'	=> 0,
    'cleansrc'	=> 0, 'precleansrc'	=> 0, 'postcleansrc'	=> 0,
    'cleanobj'	=> 0, 'precleanobj'	=> 0, 'postcleanobj'	=> 0,
    'cleaninst'	=> 0, 'precleaninst'	=> 0, 'postcleaninst'	=> 0,
    'cleanobj'	=> 0, 'precleanobj'	=> 0, 'postcleanobj'	=> 0,
    'update'	=> 0,
    'patch'	=> 0,
    'world'	=> 0,
    'lint'	=> 0,
    'kernels'	=> 0,
    'install'	=> 0,
    'release'	=> 0,
    'version'	=> 0,
);
my %kernels;
my %lint;

my $starttime;

my $unamecmd = '/usr/bin/uname';

my @cvscmds = (
    '/usr/bin/cvs',
    '/usr/local/bin/cvs',
);

my @cvsupcmds = (
    '/usr/bin/csup',
    '/usr/local/bin/csup',
    '/usr/local/bin/cvsup'
);

my @svncmds = (
    '/usr/bin/svn',
    '/usr/local/bin/svn',
);

my @svnversioncmds = (
    '/usr/bin/svnversion',
    '/usr/local/bin/svnversion',
);

my $svnattempts = 4;

BEGIN {
    ($starttime) = POSIX::times();
}

END {
    my ($endtime, $user, $system, $cuser, $csystem) = POSIX::times();
    $user += $cuser;
    $system += $csystem;
    my $ticks = POSIX::sysconf(&POSIX::_SC_CLK_TCK);
    message(sprintf("%.2f user %.2f system %.2f real",
	$user / $ticks, $system / $ticks, ($endtime - $starttime) / $ticks));
}

#
# Issue a message
#
sub message(@) {

    my $time = strftime("%Y-%m-%d %H:%M:%S", localtime());
    my $msg = join(' ', @_);
    chomp($msg);
    print(STDERR "TB --- $time - $msg\n");
}

#
# Issue a warning message
#
sub warning(@) {

    message("WARNING:", @_);
    return undef;
}

#
# Issue an error message and die
#
sub error(@) {

    message("ERROR:", @_);
    exit(1);
}

#
# Invoked when ::warn() is called
#
sub sigwarn {

    warning(@_);
}

#
# Invoked when ::die() is called
#
sub sigdie {

    error(@_);
}

#
# Log that we are entering a new stage
#
sub logstage($) {
    my $msg = shift;

    chomp($msg);
    $0 = "tinderbox: [$branch $arch/$machine] $msg";
    message($msg);
}

#
# Log a copy of the environment
#
sub logenv() {

    foreach my $key (sort(keys(%ENV))) {
	message("$key=$ENV{$key}");
    }
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
# Remove a directory and all its subdirectories
#
sub remove_dir($);
sub remove_dir($) {
    my $dir = shift;

    if (-l $dir || !-d $dir) {
	print("rm $dir\n")
	    if ($verbose);
	if (!unlink($dir) && $! != ENOENT) {
	    return warning("$dir: $!");
	}
	return 1;
    }

    opendir(my $dh, $dir)
	or return warning("$dir: $!");
    my @entries = readdir($dh);
    closedir($dh)
	or return warning("$dir: $!");
    foreach my $ent (@entries) {
	next if ($ent eq '.' || $ent eq '..');
	$ent =~ m/(.*)/;
	remove_dir("$dir/$1")
	    or return undef;
    }
    print("rmdir $dir\n")
	if ($verbose);
    rmdir($dir)
	or return warning("$dir: $!");
    return 1;
}

#
# Create a directory and the path leading up to it
#
sub make_dir($);
sub make_dir($) {
    my $dir = shift;

    if (!-d $dir && $dir =~ m|^(\S*)/([^\s/]+)$|) {
	make_dir($1)
	    or return undef;
	message("mkdir $dir");
	if (!mkdir("$dir") && $! != EEXIST) {
	    return undef;
	}
    }
    return 1;
}

#
# Change working directory
#
sub cd($) {
    my $dir = shift;

    message("cd $dir");
    chdir($dir)
	or error("$dir: $!");
}

#
# Spawn a child and wait for it to finish
#
sub spawn($@) {
    my $cmd = shift;		# Command to run
    my @args = @_;		# Arguments

    message($cmd, @args);
    my $pid = fork();
    if (!defined($pid)) {
	return warning("fork(): $!");
    } elsif ($pid == 0) {
	exec($cmd, @args);
	die("exec(): $!\n");
    }
    $children{$pid} = $pid;
    my $ret = waitpid($pid, 0);
    delete $children{$pid};
    if ($ret == -1) {
	return warning("waitpid(): $!\n");
    } elsif ($? & 0xff) {
	return warning("$cmd caught signal ", $? & 0x7f, "\n");
    } elsif ($? >> 8) {
	return warning("$cmd returned exit code ", $? >> 8, "\n");
    }
    return 1;
}

#
# Run make
#
sub make($@) {
    my $target = shift;
    my %env = @_;

    my @args = map({ "$_=$env{$_}" } keys(%env));
    return spawn('/usr/bin/make',
	($jobs > 1) ? "-j$jobs" : "-B",
	$target, @args);
}

sub timeout() {
    kill(15, keys(%children))
	if (%children);
    error("timed out after $timeout seconds");
    exit(1);
}

#
# Handle the various "clean" commands
#
sub do_clean(;$) {
    my ($prepost) = @_;

    my $clean = $prepost ? "${prepost}clean" : "clean";

    if ($cmds{$clean} || $cmds{$clean.'src'}) {
	logstage("cleaning the source tree");
	if (-e $srcdir) {
	    remove_dir($srcdir)
		or error("unable to remove old source directory");
	}
    }
    if ($cmds{$clean} || $cmds{$clean.'obj'}) {
	logstage("cleaning the object tree");
	if (-e $objdir) {
	    remove_dir($objdir)
		or error("unable to remove old object directory");
	}
    }
    if ($cmds{$clean} || $cmds{$clean.'inst'}) {
	logstage("cleaning the installation tree");
	if (-e $destdir) {
	    spawn('/bin/chflags', '-R', '0', $destdir);
	    remove_dir($destdir)
		or error("unable to remove old installation directory");
	}
    }
    if ($cmds{$clean} || $cmds{$clean.'root'}) {
	logstage("cleaning the chroot tree");
	if (-e "$sandbox/root") {
	    spawn('/bin/chflags', '-R', '0', "$sandbox/root");
	    remove_dir("$sandbox/root")
		or error("unable to remove old chroot directory");
	}
    }
}

sub usage() {

    print(STDERR "This is the BSD tinderbox script, version $VERSION.
$COPYRIGHT

Usage:
  $0 [options] [parameters] command [...]

Options:
  --verbose                     Verbose mode

Parameters:
  --arch=ARCH                   Target architecture (e.g. i386)
  --branch=BRANCH               CVS branch to check out
  --cvsup                       CVSup server
  --destdir=DIR                 Destination directory when installing
  --jobs=NUM                    Maximum number of paralell jobs
  --hostname=NAME               Name of the host running the tinderbox
  --logfile=FILE                Path to log file
  --machine=MACHINE             Target machine (e.g. pc98)
  --patch=PATCH                 Patch to apply before building
  --repository=DIR              Location of CVS repository
  --sandbox=DIR                 Location of sandbox
  --svnbase=URL                 Subversion base URL
  --timeout=SECONDS             Maximum allowed build time
  --verbose                     Increase log detail

Commands:
  clean                         Clean the sandbox
  cleansrc                      Clean the source tree
  cleanobj                      Clean the object tree
  cleaninst                     Clean the installation tree
  cleanroot                     Clean the release chroot
  update                        Update the source tree
  patch                         Patch the source tree
  world                         Build the world
  kernel:KERNCONF               Build the KERNCONF kernel
  lint                          Build the LINT kernel
  install                       Install world and all kernels
  release                       Build a full release (run as root!)

");
    exit(1);
}

MAIN:{
    # Clear environment and set timezone
    %ENV = (
	'TZ'		=> "UTC",
	'PATH'		=> "/usr/bin:/usr/sbin:/bin:/sbin",
    );
    tzset();

    # Set defaults
    $hostname = `$unamecmd -n`;
    chomp($hostname);
    $branch = "HEAD";
    $jobs = 0;
    $repository = "/home/cvs";
    $sandbox = "/tmp/tinderbox";
    $timeout = 0;

    # Get options
    GetOptions(
	"arch=s"		=> \$arch,
	"branch=s"		=> \$branch,
	"cvsup=s"		=> \$cvsup,
	"destdir=s"		=> \$destdir,
	"jobs=i"		=> \$jobs,
	"hostname=s"		=> \$hostname,
	"logfile=s"		=> \$logfile,
	"machine=s"		=> \$machine,
	"objdir=s"		=> \$objdir,
	"patch=s"		=> \$patch,
	"repository=s"		=> \$repository,
	"sandbox=s"		=> \$sandbox,
	"srcdir=s"		=> \$srcdir,
	"svnbase=s"		=> \$svnbase,
	"timeout=i"		=> \$timeout,
	"verbose+"		=> \$verbose,
	) or usage();

    if ($jobs < 0) {
	error("invalid number of jobs");
    }
    if ($timeout < 0) {
	error("invalid timeout");
    }
    if ($branch !~ m|^(\w+)$|) {
	error("invalid source branch");
    }
    $branch = ($1 eq 'CURRENT') ? 'HEAD' : $1;
    if (!defined($arch)) {
	$arch = `$unamecmd -p`;
	chomp($arch);
	if (!defined($machine)) {
	    $machine = `$unamecmd -m`;
	    chomp($machine);
	}
    }
    if ($arch !~ m|^(\w+)$|) {
	error("invalid target architecture");
    }
    $arch = $1;
    if (!defined($machine)) {
	$machine = $arch;
    }
    if ($machine !~ m|^(\w+)$|) {
	error("invalid target machine");
    }
    $machine = $1;
    if (!defined($destdir)) {
	$destdir = "$sandbox/inst";
    }

    if (!@ARGV) {
	usage();
    }

    # Set up a timeout
    if ($timeout > 0) {
	$SIG{ALRM} = \&timeout;
	alarm($timeout);
    }

    # Find out what we're expected to do
    foreach my $cmd (@ARGV) {
	if ($cmd =~ m/^([0-9A-Z_]+)=(.*)\s*$/) {
	    $userenv{$1} = $2;
	    next;
	}
	if ($cmd =~ m/^kernel:(\w+)$/) {
	    $kernels{$1} = 1;
	    next;
	}
	# backward compatibility
	# note that LINT is special, GENERIC is not
	if ($cmd eq 'generic') {
	    $kernels{'GENERIC'} = 1;
	    next;
	}
	if (!exists($cmds{$cmd})) {
	    error("unrecognized command: '$cmd'");
	}
	$cmds{$cmd} = 1;
    }

    # Open logfile
    open(STDIN, '<', "/dev/null")
	or error("/dev/null: $!\n");
    if (defined($logfile)) {
	if ($logfile !~ m|([\w./-]+)$|) {
	    error("invalid log file name");
	}
	$logfile = $1;
	unlink($logfile);
	open(STDOUT, '>', $logfile)
	    or error("$logfile: $!");
    }
    open(STDERR, ">&STDOUT");
    $| = 1;
    logstage("tinderbox $VERSION running on $hostname");
    logstage(`$unamecmd -a`);
    logstage("starting $branch tinderbox run for $arch/$machine");
    $SIG{__DIE__} = \&sigdie;
    $SIG{__WARN__} = \&sigwarn;

    # Take control of our sandbox
    if ($sandbox !~ m|^(/[\w./-]+)$|) {
	error("invalid sandbox directory");
    }
    $sandbox = "$1/$branch/$arch/$machine";
    $ENV{'HOME'} = $sandbox;
    make_dir($sandbox)
	or error("$sandbox: $!");
    my $lockfile = open_locked("$sandbox/lock", ">", 0600);
    if (!defined($lockfile)) {
	error("unable to lock sandbox");
    }
    truncate($lockfile, 0);
    print($lockfile "$$\n");

    if (defined($srcdir)) {
	if ($srcdir !~ m|^(/[\w./-]+)$|) {
	    error("invalid source directory");
	}
	$srcdir = $1;
    } else {
	$srcdir = "$sandbox/src";
    }

    if (defined($objdir)) {
	if ($objdir !~ m|^(/[\w./-]+)$|) {
	    error("invalid object directory");
	}
	$objdir = $1;
    } else {
	$objdir = "$sandbox/obj";
    }

    # Clean up remains from old runs
    do_clean(); # no prefix for backward compatibility
    do_clean('pre');

    # Check out new source tree
    if ($cmds{'update'}) {
	if (defined($svnbase)) {
	    my @svnargs;
	    push(@svnargs, "--quiet")
		unless ($verbose);
	    # ugly-bugly magic required because CVS to SVN conversion
	    # smashed branch names
	    $svnbase =~ s/\/$//;
	    if ($branch eq 'HEAD') {
		$svnbase .= '/head';
	    } elsif ($branch =~ m/^RELENG_(\d+)_(\d+)$/) {
		$svnbase .= "/releng/$1.$2";
	    } elsif ($branch =~ m/^RELENG_(\d+)$/) {
		$svnbase .= "/stable/$1";
	    } else {
		error("unrecognized branch: $branch");
	    }
	    logstage("checking out $srcdir from $svnbase");
	    my $svncmd = [grep({ -x } @svncmds)]->[0]
		or error("unable to locate svn binary");
	    cd("$sandbox");
	    if (-d $srcdir) {
		spawn($svncmd, "cleanup", $srcdir);
		push(@svnargs, "update", $srcdir);
	    } else {
		push(@svnargs, "checkout", $svnbase, $srcdir);
	    }
	    for (0..$svnattempts) {
		last if spawn($svncmd, @svnargs);
		error("unable to check out the source tree")
		    if ($_ == $svnattempts);
		my $delay = 30 * ($_ + 1);
		warning("sleeping $delay s and retrying...");
		sleep($delay);
	    }
	} elsif (defined($cvsup)) {
	    logstage("cvsupping the source tree");
	    open(my $fh, ">", "$sandbox/supfile")
		or error("$sandbox/supfile: $!");
	    print($fh "*default base=$sandbox\n");
	    print($fh "*default prefix=$sandbox\n");
	    print($fh "*default delete use-rel-suffix\n");
	    print($fh "src-all release=cvs");
	    if ($branch eq 'HEAD') {
		print($fh " tag=.");
	    } else {
		print($fh " tag=$branch");
	    }
	    print($fh "\n");
	    close($fh);
	    my @cvsupargs = (
		"-z",
		"-r 3",
		"-g",
		"-L", ($verbose ? 1 : 0),
		"-h",
		split(' ', $cvsup),
		"$sandbox/supfile"
	    );
	    my $cvsupcmd = [grep({ -x } @cvsupcmds)]->[0]
		or error("unable to locate cvsup / csup binary");
	    spawn($cvsupcmd, @cvsupargs)
		or error("unable to cvsup the source tree");
	} else {
	    logstage("checking out the source tree from $repository");
	    cd("$sandbox");
	    my @cvsargs = (
		"-f",
		"-R",
		$verbose ? "-q" : "-Q",
		"-d$repository",
	    );
	    if (-d $srcdir) {
		push(@cvsargs, "update", "-Pd");
	    } else {
		push(@cvsargs, "checkout", "-P");
	    };
	    push(@cvsargs, ($branch eq 'HEAD') ? "-A" : "-r$branch")
		if defined($branch);
	    push(@cvsargs, "src");
	    my $cvscmd = [grep({ -x } @cvscmds)]->[0]
		or error("unable to locate cvs binary");
	    spawn($cvscmd, @cvsargs)
		or error("unable to check out the source tree");
	}
    }

    # Patch sources
    if ($cmds{'patch'} && !defined($patch)) {
	warning("no patch specified");
	$cmds{'patch'} = 0;
    }
    if ($cmds{'patch'}) {
	$patch = "$sandbox/$patch"
	    unless ($patch =~ m|^/|);
	if ($patch !~ m|^(/[\w./-]+)$|) {
	    error("invalid patchfile path");
	}
	$patch = $1;
	if (-f $patch) {
	    logstage("patching the sources");
	    cd($srcdir);
	    spawn('/usr/bin/patch', "-f", "-E", "-p0", "-s", "-i$patch")
		or error("failed to apply patch to source tree");
	} else {
	    warning("$patch does not exist");
	}
    }

    # Print source tree version information
    if ($cmds{'version'}) {
	if (defined($svnbase)) {
	    my $svncmd = [grep({ -x } @svncmds)]->[0]
		or error("unable to locate svn binary");
	    my $svnversioncmd = [grep({ -x } @svnversioncmds)]->[0]
		or error("unable to locate svnversion binary");
	    if ($verbose) {
		spawn($svncmd, "stat", $srcdir)
		    or error("unable to stat source tree");
	    }
	    my $svnversion = `$svnversioncmd $srcdir`; # XXX
	    message("At svn revision $svnversion");
	} else {
	    warning("the 'version' target is only supported for svn");
	}
    }

    # Prepare environment for make(1);
    %ENV = (
	'TZ'			=> "UTC",
	'PATH'			=> "/usr/bin:/usr/sbin:/bin:/sbin",

	'__MAKE_CONF'		=> "/dev/null",
	'SRCCONF'		=> "/dev/null",
	'MAKEOBJDIRPREFIX'	=> $objdir,

	'TARGET'		=> $machine,
	'TARGET_ARCH'		=> $arch,

	# Force cross-build, even when host == target
	'CROSS_BUILD_TESTING'	=> "YES",
    );

    # Kernel-specific variables
    if (%kernels || $cmds{'lint'} || $cmds{'release'}) {
	# None at the moment
    }

    # User-supplied variables
    foreach my $key (keys(%userenv)) {
	if (exists($ENV{$key})) {
	    warning("will not allow override of $key");
	} else {
	    $ENV{$key} = $userenv{$key};
	}
    }

    # Makefile.inc1 makes the idiotic assumption that you could not ever
    # start a build less than one second after the source tree was updated.
    sleep(1);

    # Build the world, or at least the kernel toolchain
    if ($cmds{'world'}) {
	logstage("building world");
	logenv();
	cd($srcdir);
	make('buildworld')
	    or error("failed to build world");
    } elsif (%kernels || $cmds{'lint'} || $cmds{'kernels'}) {
	logstage("building kernel toolchain");
	logenv();
	cd($srcdir);
	make('kernel-toolchain')
	    or error("failed to build kernel toolchain");
    }

    # Locate LINT configs if requested
    if ($cmds{'lint'}) {
	if (-f "$srcdir/sys/$machine/conf/NOTES") {
	    logstage("generating LINT kernel config");
	    cd("$srcdir/sys/$machine/conf");
	    make('LINT')
		or error("failed to generate LINT kernel config");
	    my $dir = "$srcdir/sys/$machine/conf/";
	    if (opendir(my $dh, $dir)) {
		foreach (readdir($dh)) {
		    if ($_ =~ m|^(LINT(?:-[A-Z0-9][A-Z0-9_.-]*[A-Z0-9])?)$| && -f "$dir/$_") {
			$lint{$1} = 1;
		    }
		}
		closedir($dh);
	    } else {
		warning("$dir: $!\n");
	    }
	}
    }

    # Locate additional kernel configs
    if ($cmds{'kernels'}) {
	my $dir = "$srcdir/sys/$machine/conf/";
	if (opendir(my $dh, $dir)) {
	    foreach (readdir($dh)) {
		if ($_ =~ m|^([A-Z0-9][A-Z0-9_.-]*[A-Z0-9])$| &&
		    $_ !~ m|^LINT| &&
		    $_ ne "DEFAULTS" &&
		    $_ ne "NOTES" &&
		    -f "$dir/$_") {
		    $kernels{$1} = 1;
		}
	    }
	    closedir($dh);
	} else {
	    warning("$dir: $!\n");
	}
    }

  kernel:
    foreach my $kernel (sort(keys(%lint)), sort(keys(%kernels))) {
	if (! -f "$srcdir/sys/$machine/conf/$kernel") {
	    warning("no kernel config for $kernel");
	    next kernel;
	}
	# Check that the config is appropriate for this target.
	cd("$srcdir/sys/$machine/conf");
	local *PIPE;
	my @cmdline = ("/usr/sbin/config", "-m", "$kernel");
	message(@cmdline);
	if (open(PIPE, "-|", @cmdline)) {
	    local $/;
	    my $config_m = <PIPE>;
	    close(PIPE);
	    print($config_m)
		if ($verbose);
	    if ($config_m !~ m|^\Q$machine\E\s+\Q$arch\E\s*$|s) {
		message("skipping $kernel kernel");
		next kernel;
	    }
	} else {
	    warning("$kernel: $!");
	    next kernel;
	}
	logstage("building $kernel kernel");
	logenv();
	cd($srcdir);
	make('buildkernel', 'KERNCONF' => $kernel)
	    or error("failed to build $kernel kernel");
    }

    # Install world and kernel if requested
    if ($cmds{'install'}) {
	make_dir($destdir)
	    or error("$destdir: $!");
	cd($srcdir);
	make('installworld', 'DESTDIR' => $destdir)
	    or error("failed to install world");
	foreach my $kernel (sort(keys(%kernels))) {
	    if (! -f "$srcdir/sys/$machine/conf/$kernel") {
		warning("no kernel config for $kernel");
		next;
	    }
	    make('installkernel',
		 'KERNCONF' => $kernel,
		 'KODIR' => "/boot/$kernel",
		 'DESTDIR' => $destdir);
	}
    }

    # Build a release if requested
    if ($cmds{'release'}) {
	$ENV{'CHROOTDIR'} = "$sandbox/root";
	$ENV{'CVSROOT'} = $repository;
	$ENV{'RELEASETAG'} = $branch
	    if $branch ne 'HEAD';
	$ENV{'WORLD_FLAGS'} = $ENV{'KERNEL_FLAGS'} =
	    ($jobs > 1) ? "-j$jobs" : "-B";
	if ($patch) {
	    $ENV{'LOCAL_PATCHES'} = $patch;
	    $ENV{'PATCH_FLAGS'} = "-fs";
	}

	# Save time and space
	$ENV{'NOCDROM'} = "YES";
	$ENV{'NODOC'} = "YES";
	$ENV{'NOPORTS'} = "YES";

	logstage("building a release");
	logenv();
	cd("$srcdir/release");
	make('release')
	    or error("failed to build release");
    }

    # Clean up after us
    do_clean('post');

    # Exiting releases the lock file
    logstage("tinderbox run completed");
    exit(0);
}
