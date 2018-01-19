#!/usr/bin/env perl
#
# $Id: regress.pl,v 1.8 2017/07/18 18:47:06 schwarze Exp $
#
# Copyright (c) 2017 Ingo Schwarze <schwarze@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use warnings;
use strict;

# Used because open(3p) and open2(3p) provide no way for handling
# STDERR of the child process, neither for appending it to STDOUT,
# nor for piping it into the Perl program.
use IPC::Open3 qw(open3);

# Define this at one place such that it can easily be changed
# if diff(1) does not support the -a option.
my @diff = qw(diff -au);

# --- utility functions ------------------------------------------------

sub usage ($) {
	warn shift;
	print STDERR "usage: $0 [directory[:test] [modifier ...]]\n";
	exit 1;
}

# Modifier arguments provided on the command line,
# inspected by the main program and by the utility functions.
my %targets;

# Run a command and send STDOUT and STDERR to a file.
# 1st argument: path to the output file
# 2nd argument: command name
# The remaining arguments are passed to the command.
sub sysout ($@) {
	my $outfile = shift;
	print "@_\n" if $targets{verbose};
	local *OUT_FH;
	open OUT_FH, '>', $outfile or die "$outfile: $!";
	my $pid = open3 undef, ">&OUT_FH", undef, @_;
	close OUT_FH;
	waitpid $pid, 0;
	return $? >> 8;
}

# Simlar, but filter the output as needed for the lint test.
sub syslint ($@) {
	my $outfile = shift;
	print "@_\n" if $targets{verbose};
	open my $outfd, '>', $outfile or die "$outfile: $!";
	my $infd;
	my $pid = open3 undef, $infd, undef, @_;
	while (<$infd>) {
		s/^mandoc: [^:]+\//mandoc: /;
		print $outfd $_;
	}
	close $outfd;
	close $infd;
	waitpid $pid, 0;
	return 0;
}

# Simlar, but filter the output as needed for the html test.
sub syshtml ($@) {
	my $outfile = shift;
	print "@_\n" if $targets{verbose};
	open my $outfd, '>', $outfile or die "$outfile: $!";
	my $infd;
	my $pid = open3 undef, $infd, undef, @_;
	my $state;
	while (<$infd>) {
		chomp;
		if (!$state && s/.*<math class="eqn">//) {
			$state = 1;
			next unless length;
		}
		$state = 1 if /^BEGINTEST/;
		if ($state && s/<\/math>.*//) {
			s/^ *//;
			print $outfd "$_\n" if length;
			undef $state;
			next;
		}
		s/^ *//;
		print $outfd "$_\n" if $state;
		undef $state if /^ENDTEST/;
	}
	close $outfd;
	close $infd;
	waitpid $pid, 0;
	return 0;
}

my @failures;
sub fail ($$) {
	warn "FAILED: @_\n";
	push @failures, [@_];
}


# --- process command line arguments -----------------------------------

my $onlytest = shift // '';
for (@ARGV) {
	/^(all|ascii|utf8|man|html|markdown|lint|clean|verbose)$/
	    or usage "$_: invalid modifier";
	$targets{$_} = 1;
}
$targets{all} = 1
    unless $targets{ascii} || $targets{utf8} || $targets{man} ||
      $targets{html} || $targets{markdown} ||
      $targets{lint} || $targets{clean};
$targets{ascii} = $targets{utf8} = $targets{man} = $targets{html} =
    $targets{markdown} = $targets{lint} = 1 if $targets{all};


# --- parse Makefiles --------------------------------------------------

sub parse_makefile ($%) {
	my ($filename, $vars) = @_;
	open my $fh, '<', $filename or die "$filename: $!";
	while (<$fh>) {
		chomp;
		next unless /\S/;
		last if /^# OpenBSD only/;
		next if /^#/;
		next if /^\.include/;
		/^(\w+)\s*([?+]?)=\s*(.*)/
		    or die "$filename: parse error: $_";
		my $var = $1;
		my $opt = $2;
		my $val = $3;
		$val =~ s/\$\{(\w+)\}/$vars->{$1}/;
		$val = "$vars->{$var} $val" if $opt eq '+';
		$vars->{$var} = $val
		    unless $opt eq '?' && defined $vars->{$var};
	}
	close $fh;
}

my (@regress_tests, @utf8_tests, @lint_tests, @html_tests);
my (%skip_ascii, %skip_man, %skip_markdown);
foreach my $module (qw(roff char mdoc man tbl eqn)) {
	my %modvars;
	parse_makefile "$module/Makefile", \%modvars;
	foreach my $subdir (split ' ', $modvars{SUBDIR}) {
		my %subvars = (MOPTS => '');
		parse_makefile "$module/$subdir/Makefile", \%subvars;
		parse_makefile "$module/Makefile.inc", \%subvars;
		delete $subvars{SKIP_GROFF};
		delete $subvars{SKIP_GROFF_ASCII};
		delete $subvars{TBL};
		delete $subvars{EQN};
		my @mandoc = ('../mandoc', split ' ', $subvars{MOPTS});
		delete $subvars{MOPTS};
		my @regress_testnames;
		if (defined $subvars{REGRESS_TARGETS}) {
			push @regress_testnames,
			    split ' ', $subvars{REGRESS_TARGETS};
			push @regress_tests, {
			    NAME => "$module/$subdir/$_",
			    MANDOC => \@mandoc,
			} foreach @regress_testnames;
			delete $subvars{REGRESS_TARGETS};
		}
		if (defined $subvars{UTF8_TARGETS}) {
			push @utf8_tests, {
			    NAME => "$module/$subdir/$_",
			    MANDOC => \@mandoc,
			} foreach split ' ', $subvars{UTF8_TARGETS};
			delete $subvars{UTF8_TARGETS};
		}
		if (defined $subvars{HTML_TARGETS}) {
			push @html_tests, {
			    NAME => "$module/$subdir/$_",
			    MANDOC => \@mandoc,
			} foreach split ' ', $subvars{HTML_TARGETS};
			delete $subvars{HTML_TARGETS};
		}
		if (defined $subvars{LINT_TARGETS}) {
			push @lint_tests, {
			    NAME => "$module/$subdir/$_",
			    MANDOC => \@mandoc,
			} foreach split ' ', $subvars{LINT_TARGETS};
			delete $subvars{LINT_TARGETS};
		}
		if (defined $subvars{SKIP_ASCII}) {
			for (split ' ', $subvars{SKIP_ASCII}) {
				$skip_ascii{"$module/$subdir/$_"} = 1;
				$skip_man{"$module/$subdir/$_"} = 1;
			}
			delete $subvars{SKIP_ASCII};
		}
		if (defined $subvars{SKIP_TMAN}) {
			$skip_man{"$module/$subdir/$_"} = 1
			    for split ' ', $subvars{SKIP_TMAN};
			delete $subvars{SKIP_TMAN};
		}
		if (defined $subvars{SKIP_MARKDOWN}) {
			$skip_markdown{"$module/$subdir/$_"} = 1
			    for split ' ', $subvars{SKIP_MARKDOWN};
			delete $subvars{SKIP_MARKDOWN};
		}
		if (keys %subvars) {
			my @vars = keys %subvars;
			die "unknown var(s) @vars in dir $module/$subdir";
		}
		map {
			$skip_ascii{"$module/$subdir/$_"} = 1;
		} @regress_testnames if $skip_ascii{"$module/$subdir/ALL"};
		map {
			$skip_man{"$module/$subdir/$_"} = 1;
		} @regress_testnames if $skip_man{"$module/$subdir/ALL"};
		map {
			$skip_markdown{"$module/$subdir/$_"} = 1;
		} @regress_testnames if $skip_markdown{"$module/$subdir/ALL"};
	}
	delete $modvars{SUBDIR};
	if (keys %modvars) {
		my @vars = keys %modvars;
		die "unknown var(s) @vars in module $module";
	}
}

# --- run targets ------------------------------------------------------

my $count_total = 0;
my $count_ascii = 0;
my $count_man = 0;
my $count_rm = 0;
if ($targets{ascii} || $targets{man}) {
	print "Running ascii and man tests ";
	print "...\n" if $targets{verbose};
}
for my $test (@regress_tests) {
	my $i = "$test->{NAME}.in";
	my $o = "$test->{NAME}.mandoc_ascii";
	my $w = "$test->{NAME}.out_ascii";
	if ($targets{ascii} && !$skip_ascii{$test->{NAME}} &&
	    $test->{NAME} =~ /^$onlytest/) {
		$count_ascii++;
		$count_total++;
		sysout $o, @{$test->{MANDOC}}, qw(-I os=OpenBSD -T ascii), $i
		    and fail $test->{NAME}, 'ascii:mandoc';
		system @diff, $w, $o
		    and fail $test->{NAME}, 'ascii:diff';
		print "." unless $targets{verbose};
	}
	my $m = "$test->{NAME}.in_man";
	my $mo = "$test->{NAME}.mandoc_man";
	if ($targets{man} && !$skip_man{$test->{NAME}} &&
	    $test->{NAME} =~ /^$onlytest/) {
		$count_man++;
		$count_total++;
		sysout $m, @{$test->{MANDOC}}, qw(-I os=OpenBSD -T man), $i
		    and fail $test->{NAME}, 'man:man';
		sysout $mo, @{$test->{MANDOC}},
		    qw(-man -I os=OpenBSD -T ascii -O mdoc), $m
		    and fail $test->{NAME}, 'man:mandoc';
		system @diff, $w, $mo
		    and fail $test->{NAME}, 'man:diff';
		print "." unless $targets{verbose};
	}
	if ($targets{clean}) {
		print "rm $o $m $mo\n" if $targets{verbose};
		$count_rm += unlink $o, $m, $mo;
	}
}
if ($targets{ascii} || $targets{man}) {
	print "Number of ascii and man tests:" if $targets{verbose};
	print " $count_ascii + $count_man tests run.\n";
}

my $count_utf8 = 0;
if ($targets{utf8}) {
	print "Running utf8 tests ";
	print "...\n" if $targets{verbose};
}
for my $test (@utf8_tests) {
	my $i = "$test->{NAME}.in";
	my $o = "$test->{NAME}.mandoc_utf8";
	my $w = "$test->{NAME}.out_utf8";
	if ($targets{utf8} && $test->{NAME} =~ /^$onlytest/o) {
		$count_utf8++;
		$count_total++;
		sysout $o, @{$test->{MANDOC}}, qw(-I os=OpenBSD -T utf8), $i
		    and fail $test->{NAME}, 'utf8:mandoc';
		system @diff, $w, $o
		    and fail $test->{NAME}, 'utf8:diff';
		print "." unless $targets{verbose};
	}
	if ($targets{clean}) {
		print "rm $o\n" if $targets{verbose};
		$count_rm += unlink $o;
	}
}
if ($targets{utf8}) {
	print "Number of utf8 tests:" if $targets{verbose};
	print " $count_utf8 tests run.\n";
}

my $count_html = 0;
if ($targets{html}) {
	print "Running html tests ";
	print "...\n" if $targets{verbose};
}
for my $test (@html_tests) {
	my $i = "$test->{NAME}.in";
	my $o = "$test->{NAME}.mandoc_html";
	my $w = "$test->{NAME}.out_html";
	if ($targets{html} && $test->{NAME} =~ /^$onlytest/) {
		$count_html++;
		$count_total++;
		syshtml $o, @{$test->{MANDOC}}, qw(-T html), $i
		    and fail $test->{NAME}, 'html:mandoc';
		system @diff, $w, $o
		    and fail $test->{NAME}, 'html:diff';
		print "." unless $targets{verbose};
	}
	if ($targets{clean}) {
		print "rm $o\n" if $targets{verbose};
		$count_rm += unlink $o;
	}
}
if ($targets{html}) {
	print "Number of html tests:" if $targets{verbose};
	print " $count_html tests run.\n";
}

my $count_markdown = 0;
if ($targets{markdown}) {
	print "Running markdown tests ";
	print "...\n" if $targets{verbose};
}
for my $test (@regress_tests) {
	my $i = "$test->{NAME}.in";
	my $o = "$test->{NAME}.mandoc_markdown";
	my $w = "$test->{NAME}.out_markdown";
	if ($targets{markdown} && !$skip_markdown{$test->{NAME}} &&
	    $test->{NAME} =~ /^$onlytest/) {
		$count_markdown++;
		$count_total++;
		sysout $o, @{$test->{MANDOC}},
		    qw(-I os=OpenBSD -T markdown), $i
		    and fail $test->{NAME}, 'markdown:mandoc';
		system @diff, $w, $o
		    and fail $test->{NAME}, 'markdown:diff';
		print "." unless $targets{verbose};
	}
	if ($targets{clean}) {
		print "rm $o\n" if $targets{verbose};
		$count_rm += unlink $o;
	}
}
if ($targets{markdown}) {
	print "Number of markdown tests:" if $targets{verbose};
	print " $count_markdown tests run.\n";
}

my $count_lint = 0;
if ($targets{lint}) {
	print "Running lint tests ";
	print "...\n" if $targets{verbose};
}
for my $test (@lint_tests) {
	my $i = "$test->{NAME}.in";
	my $o = "$test->{NAME}.mandoc_lint";
	my $w = "$test->{NAME}.out_lint";
	if ($targets{lint} && $test->{NAME} =~ /^$onlytest/) {
		$count_lint++;
		$count_total++;
		syslint $o, @{$test->{MANDOC}},
		    qw(-I os=OpenBSD -T lint -W all), $i
		    and fail $test->{NAME}, 'lint:mandoc';
		system @diff, $w, $o
		    and fail $test->{NAME}, 'lint:diff';
		print "." unless $targets{verbose};
	}
	if ($targets{clean}) {
		print "rm $o\n" if $targets{verbose};
		$count_rm += unlink $o;
	}
}
if ($targets{lint}) {
	print "Number of lint tests:" if $targets{verbose};
	print " $count_lint tests run.\n";
}

# --- final report -----------------------------------------------------

if (@failures) {
	print "\nNUMBER OF FAILED TESTS: ", scalar @failures,
	    " (of $count_total tests run.)\n";
	print "@$_\n" for @failures;
	print "\n";
	exit 1;
}
print "\n" if $targets{verbose};
if ($count_total == 1) {
	print "Test succeeded.\n";
} elsif ($count_total) {
	print "All $count_total tests OK:";
	print " $count_ascii ascii" if $count_ascii;
	print " $count_man man" if $count_man;
	print " $count_utf8 utf8" if $count_utf8;
	print " $count_html html" if $count_html;
	print " $count_markdown markdown" if $count_markdown;
	print " $count_lint lint" if $count_lint;
	print "\n";
} else {
	print "No tests were run.\n";
} 
if ($targets{clean}) {
	if ($count_rm) {
		print "Deleted $count_rm test output files.\n";
		print "The tree is now clean.\n";
	} else {
		print "No test output files were found.\n";
		print "The tree was already clean.\n";
	}
}
exit 0;
