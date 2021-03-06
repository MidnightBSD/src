#!/usr/bin/perl -Tw
#-
# Copyright (c) 2003-2013 Dag-Erling Smørgrav
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
# $FreeBSD: user/des/tinderbox/www/index.cgi 256311 2013-10-11 08:53:27Z des $
#

use v5.10.1;
use strict;
use POSIX qw(strftime);
use Sys::Hostname;

my %BRANCHES;
my %ARCHES;

my $DIR = ".";

sub success($) {
    my ($log) = @_;

    local *FILE;
    my $result;

    if (open(FILE, "<", $log)) {
	$result = grep { m/tinderbox run completed/ } <FILE>;
	close(FILE);
    }
    return $result;
}

sub branch_rank($) {
    my ($branch) = @_;

    my $rank;

    if ($branch =~ m/\b(HEAD|CURRENT)$/i) {
	$rank = "9999";
    } elsif ($branch =~ m/\bRELENG_(\d{1,2})$/i) {
	$rank = sprintf("%02d99", $1);
    } elsif ($branch =~ m/\bRELENG_(\d{1,2})_(\d{1,2})$/i) {
	$rank = sprintf("%02d%02d", $1, $2);
    } else {
	$rank = $branch;
    }
    return $rank;
}

sub branch_sort($$) {
    my ($a, $b) = @_;

    return branch_rank($a) cmp branch_rank($b);
}

sub inverse_branch_sort($$) {
    my ($a, $b) = @_;

    return branch_rank($b) cmp branch_rank($a);
}

sub do_branch($) {
    my ($branch) = @_;

    my $prettybranch = $branch;
    $prettybranch =~ s@^HEAD$@head@;
    $prettybranch =~ s@^RELENG_(\d+)_(\d+)$@releng/$1.$2@;
    $prettybranch =~ s@^RELENG_(\d+)$@stable/$1@;

    print "      <tr class='header'>
        <th>&nbsp;</th>
";
    foreach my $arch (sort(keys(%ARCHES))) {
	foreach my $machine (sort(keys(%{$ARCHES{$arch}}))) {
	    if ($arch eq $machine) {
		print "        <th>$arch</th>\n";
	    } else {
		print "        <th>$arch<br />$machine</th>\n";
	    }
	}
    }
    print "      </tr>\n";

    my $now = time();

    foreach my $config (sort(keys(%{$BRANCHES{$branch}}))) {
	$config =~ m/^(\w+)((?:-\w+)*?)(-build)?$/;
	my $variant = $2 =~ s/^-//r;
	print "      <tr>
	<th>$prettybranch" . ($variant ? "<br/>($variant)" : "") . "</th>
";
	foreach my $arch (sort(keys(%ARCHES))) {
	    foreach my $machine (sort(keys(%{$ARCHES{$arch}}))) {
		my $log = "tinderbox-$config-$branch-$arch-$machine";
		if (-f "$DIR/$log.brief") {
		    print "        <td align='center' class='result'>";
		    my @stat = stat(_);
		    my $class = success("$DIR/$log.brief") ? "ok" : "fail";
		    my $age = int(($now - $stat[9]) / 1800);
		    $age = ($age < 0) ? 0 : ($age > 9) ? 9 : $age;
		    $class .= "-$age";
		    print "<span class='$class'>" .
			strftime("%Y-%m-%d<br />%H:%M&nbsp;UTC", gmtime($stat[9])) .
			"</span><br />";
		    print "<span class='tiny'>" .
			"<a href='$log.brief'>summary</a>";
		    if (-f "$DIR/$log.full") {
			print " | <a href='$log.full'>full&nbsp;log</a>";
		    }
		    print "</span>";
		    print "</td>\n";
		} else {
		    print "        <td align='center' class='noresult'>n/a</td>\n";
		}
	    }
	}
	print "      </tr>\n";
    }
}

MAIN:{
    my $date = strftime("%Y-%m-%d %H:%M:%S UTC", gmtime());
    my $realthing; # is this the authentic tinderbox site
    my $greeting;

    $| = 1;
    if ($ENV{'GATEWAY_INTERFACE'}) {
	print "Content-Type: text/html; charset=utf-8\n\n";
	$realthing = ($ENV{'SERVER_NAME'} eq 'tinderbox.freebsd.org');
    } else {
	my $host = hostname();
	$realthing = ($host eq 'dma.des.no');
    }

    if ($realthing) {
	$greeting = "<a href='http://tinderbox.freebsd.org/'>tinderbox.freebsd.org</a>";
    } else {
	$greeting = "For official Tinderbox logs, see <a href='http://tinderbox.freebsd.org/'>here</a>";
    }

    local *DIR;
    opendir(DIR, $DIR)
	or die("$DIR: $!\n");
    foreach (readdir(DIR)) {
	next unless m/^tinderbox-([\w-]+)-(\w+)-(\w+)-(\w+)\.(brief|full)$/;
	$BRANCHES{$2}->{$1} = $ARCHES{$3}->{$4} = 1;
    }
    closedir(DIR);

    print "<?xml version='1.0' encoding='utf-8'?>
<!DOCTYPE html
     PUBLIC '-//W3C//DTD XHTML 1.0 Strict//EN'
     'http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd'>
<html xmlns='http://www.w3.org/1999/xhtml' xml:lang='en' lang='en'>
  <head>
    <title>FreeBSD tinderbox logs</title>
    <meta name='robots' content='nofollow' />
    <meta http-equiv='refresh' content='600' />
    <link rel='stylesheet' type='text/css' media='screen' href='tb.css' />
    <link rel='shortcut icon' type='image/x-icon' href='favicon.ico' />
  </head>
  <body>
    <!-- h1>FreeBSD tinderbox logs</h1 -->

    <table border='1'>
";

    # Count columns
    my $columns = 1;
    print "      <col id='col-config' />\n";
    foreach my $arch (sort(keys(%ARCHES))) {
	foreach my $machine (sort(keys(%{$ARCHES{$arch}}))) {
	    print "      <col id='col-$arch-$machine' />\n";
	    $columns++;
	}
    }

    # Generate rows
    foreach my $branch (sort(inverse_branch_sort keys(%BRANCHES))) {
	do_branch($branch);
    }

    print "
      <tr class='footer'>
        <td colspan='$columns'>
        <div class='footer-left'>$date</div>
        <div class='footer-right'>$greeting</div>
        </td>
      </tr>
    </table>
    <!-- p>
      <a href='http://validator.w3.org/check/referer'><img
          src='valid-xhtml10.png'
          alt='Valid XHTML 1.0!' height='31' width='88' /></a>
      <a href='http://jigsaw.w3.org/css-validator/check/referer'><img
          src='valid-css.png'
          alt='Valid CSS!' height='31' width='88' /></a>
    </p -->
  </body>
</html>
";
    exit(0);
}
