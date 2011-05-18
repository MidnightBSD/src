#!./perl -w

# Test the well-formed-ness of the MANIFEST file.

BEGIN {
    chdir 't';
    @INC = '../lib';
}

use strict;
use File::Spec;
require './test.pl';

plan('no_plan');

my $manifest = File::Spec->catfile(File::Spec->updir(), 'MANIFEST');

open my $m, '<', $manifest or die "Can't open '$manifest': $!";
my @files;
# Test that MANIFEST uses tabs - not spaces - after the name of the file.
while (<$m>) {
    chomp;
    unless( /\s/ ) {
        push @files, $_;
        # no need for further tests on lines without whitespace (i.e., filename only)
        next;
    }
    my ($file, $separator) = /^(\S+)(\s+)/;
    push @files, $file;

    isnt($file, undef, "Line $. doesn't start with a blank") or next;
    # Remember, we're running from t/
    ok(-f "../$file", "File $file exists");
    if ($separator !~ tr/\t//c) {
	# It's all tabs
	next;
    } elsif ($separator !~ tr/ //c) {
	# It's all spaces
	fail("Spaces in entry for $file");
    } elsif ($separator =~ tr/\t//) {
	fail("Mixed tabs and spaces in entry for $file");
    } else {
	fail("Odd whitespace in entry for $file");
    }
}

close $m or die $!;

# Test that MANIFEST is properly sorted
SKIP: {
    skip("'Porting/manisort' not found", 1) if (! -f '../Porting/manisort');

    my $result = runperl('progfile' => '../Porting/manisort',
                         'args'     => [ '-c', '../MANIFEST' ],
                         'stderr'   => 1);

    like($result, qr/is sorted properly/, 'MANIFEST sorted properly');
}

SKIP: {
    chdir "..";
    skip("not under git control", 3) unless -d '.git';
    chomp(my @repo= grep { !/\.gitignore$/ } `git ls-files`);
    skip("git ls-files didnt work",3)
        if !@repo;
    is( 0+@repo, 0+@files, "git ls-files has a corresponding number of files as does MANIFEST");
    my %repo= map { $_ => 1 } @repo;
    my %mani= map { $_ => 1 } @files;
    is( 0+keys %mani, 0+@files, "no duplicate files in MANIFEST");
    delete $mani{$_} for @repo;
    delete $repo{$_} for @files;
    my @not_in_mani= keys %repo;
    my @still_in_mani= keys %mani;

    is( 0+@not_in_mani, 0, "Nothing added to the repo that isn't in MANIFEST");
    is( "not in MANIFEST: @not_in_mani", "not in MANIFEST: ",
        "Nothing added to the repo that isn't in MANIFEST");
    is( 0+@still_in_mani, 0, "Nothing in the MANIFEST that isn't tracked by git");
    is( "should not be in MANIFEST: @still_in_mani", "should not be in MANIFEST: ",
        "Nothing in the MANIFEST that isn't tracked by git");

}

# EOF
