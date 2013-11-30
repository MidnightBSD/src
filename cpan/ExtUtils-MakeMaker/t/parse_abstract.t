#!/usr/bin/perl

use strict;
use warnings;

use ExtUtils::MakeMaker;

use Test::More 'no_plan';

sub test_abstract {
    my($code, $package, $want, $name) = @_;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    my $file = "t/abstract.tmp";
    {
        open my $fh, ">", $file or die "Can't open $file";
        print $fh $code;
        close $fh;
    }

    # Hack up a minimal MakeMaker object.
    my $mm = bless { DISTNAME => $package }, "MM";
    my $have = $mm->parse_abstract($file);

    my $ok = is( $have, $want, $name );

    # Clean up the temp file, VMS style
    1 while unlink $file;

    return $ok;
}


test_abstract(<<END, "Foo", "Stuff and things", "Simple abstract");
=head1 NAME

Foo - Stuff and things
END


test_abstract(<<END, "NEXT", "Provide a pseudo-class NEXT (et al) that allows method redispatch", "Name.pm");
=head1 NAME

NEXT.pm - Provide a pseudo-class NEXT (et al) that allows method redispatch
END


test_abstract(<<END, "Compress::Raw::Zlib::FAQ", "Frequently Asked Questions about Compress::Raw::Zlib", "double dash");
=pod

Compress::Raw::Zlib::FAQ -- Frequently Asked Questions about Compress::Raw::Zlib
END


test_abstract(<<END, "Foo", "This is", "Only in POD");
# =pod

Foo - This is not in pod

=cut

Foo - This isn't in pod either

=pod

Foo - This is

Foo - So is this.
END


test_abstract(<<END, "Foo", "the abstract", "more spaces");
=pod

Foo   -  the abstract
END
