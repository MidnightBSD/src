#!/usr/bin/perl -Tw
#-
# Copyright (c) 2003 Dag-Erling Co�dan Sm�rgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD: src/tools/tools/tinderbox/www/index.cgi,v 1.25 2005/02/07 07:55:00 des Exp $
# $MidnightBSD: src/tools/tools/tinderbox/www/index.cgi,v 1.1.1.1.2.1 2008/03/04 03:06:59 laffer1 Exp $
#

use 5.006_001;
use strict;
use POSIX qw(strftime);

my %CONFIGS;
my %ARCHES;

my $DIR = ".";

sub success($) {
    my $log = shift;

    local *FILE;
    if (open(FILE, "<", $log)) {
	while (<FILE>) {
	    if (m/tinderbox run completed/) {
		close(FILE);
		return 1;
	    }
	}
	close(FILE);
    }
    return undef;
}

sub branch_sort($$) {
    my ($a, $b) = @_;

    my @a = split('_', $a);
    my @b = split('_', $b);
    while (@a || @b) {
	($a, $b) = (shift(@a), shift(@b));
	return 1 unless defined($a);
	return -1 unless defined($b);
	next if $a eq $b;
	if ($a =~ m/^\d+$/ && $b =~ m/^\d+$/) {
	    return $a <=> $b;
	} else {
	    return $a cmp $b;
	}
    }
    return 0;
}

sub inverse_branch_sort($$) {
    my ($a, $b) = @_;

    return branch_sort($b, $a);
}

sub do_config($) {
    my $config = shift;

    my %branches = %{$CONFIGS{$config}};

    print "      <tr>
        <th>$config</th>
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

    foreach my $branch (sort(inverse_branch_sort keys(%branches))) {
	my $html =  "      <tr>
	<th>$branch</th>
";
	foreach my $arch (sort(keys(%ARCHES))) {
	    foreach my $machine (sort(keys(%{$ARCHES{$arch}}))) {
		my $log = "tinderbox-$config-$branch-$arch-$machine";
		my $links = "";
		if (-f "$DIR/$log.brief") {
		    my @stat = stat("$DIR/$log.brief");
		    my $class = success("$DIR/$log.brief") ? "ok" : "fail";
		    my $age = int(($now - $stat[9]) / 1800);
		    $age = 9
			if ($age > 9);
		    $class .= "-$age";
		    $links .= "<span class='$class'>" .
			strftime("%Y-%m-%d %H:%M&nbsp;UTC", gmtime($stat[9])) .
			"</span><br />";
		    my $size = sprintf("[%.1f&nbsp;kB]", $stat[7] / 1024);
		    $links .= " <span class='tiny'>" .
			"<a target='_top' href='$log.brief'>summary&nbsp;$size</a>" .
			"</span><br />";
		}
		if (-f "$DIR/$log.full") {
		    my @stat = stat("$DIR/$log.full");
		    my $size = sprintf("[%.1f&nbsp;MB]", $stat[7] / 1048576);
		    $links .= " <span class='tiny'>" .
			"<a target='_top' href='$log.full'>full&nbsp;log&nbsp;$size</a>" .
			"</span><br />";
		}
		if ($links eq "") {
		    $html .= "        <td>n/a</td>\n";
		} else {
		    $html .= "        <td>$links</td>\n";
		}
	    }
	}
	$html .= "      </tr>\n";
	print $html;
    }
}

MAIN:{
    if ($ENV{'GATEWAY_INTERFACE'}) {
	$| = 1;
	print "Content-Type: text/html\n\n";
    } else {
	if ($0 =~ m|^(/[\w/._-]+)/[^/]+$|) {
	    $DIR = $1;
	}
	open(STDOUT, ">", "$DIR/index.html")
	    or die("index.html: $!\n");
    }

    local *DIR;
    opendir(DIR, $DIR)
	or die("$DIR: $!\n");
    foreach (readdir(DIR)) {
	next unless m/^tinderbox-(\w+)-(\w+)-(\w+)-(\w+)\.(brief|full)$/;
	$CONFIGS{$1}->{$2} = $ARCHES{$3}->{$4} = 1;
    }
    closedir(DIR);

    print "<?xml version='1.0' encoding='iso-8859-1'?>
<!DOCTYPE html
     PUBLIC '-//W3C//DTD XHTML 1.0 Strict//EN'
     'http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd'>
<html xmlns='http://www.w3.org/1999/xhtml' xml:lang='en' lang='en'>
  <head>
    <title>BSD tinderbox logs</title>
    <meta name='robots' content='nofollow' />
    <meta http-equiv='refresh' content='600' />
    <link rel='stylesheet' type='text/css' media='screen' href='tb.css' />
    <link rel='shortcut icon' type='image/png' href='daemon.png' />
  </head>
  <body>
    <!-- h1>BSD tinderbox logs</h1 -->

    <table border='1' cellpadding='3'>
";
    foreach my $config (sort(keys(%CONFIGS))) {
	next if $config =~ m/^update_/;
	do_config($config);
    }
    my $date = strftime("%Y-%m-%d %H:%M UTC", gmtime());
    print "
    </table>
    <!-- p class='update'>Last updated: $date</p -->
    <!-- p>
      <a target='_top' href='http://validator.w3.org/check/referer'><img
          src='valid-xhtml10.png'
          alt='Valid XHTML 1.0!' height='31' width='88' /></a>
      <a target='_top' href='http://jigsaw.w3.org/css-validator/check/referer'><img
          src='valid-css.png'
          alt='Valid CSS!' height='31' width='88' /></a>
    </p -->
  </body>
</html>
";
    exit(0);
}
